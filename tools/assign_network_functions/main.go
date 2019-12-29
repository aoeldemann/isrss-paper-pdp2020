package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/google/gopacket"
	"github.com/google/gopacket/layers"

	"github.com/aoeldemann/gonettrace"
)

type FlowData struct {
	nPkts         uint64
	nBytes        uint64
	nBytesPayload uint64

	actions []Action
}

type Action struct {
	id         int
	ippBase    int
	ippPayload int
	share      float64
}

var (
	flowMap       map[string]*FlowData
	flows         []*FlowData
	actions       []Action
	traceDuration time.Duration
	logInfo       = log.New(os.Stdout, "INFO: ", log.Ldate|log.Lmicroseconds)
)

func printLog(indentlevel int, msg string, a ...interface{}) {
	for i := 0; i < indentlevel; i++ {
		msg = "... " + msg
	}
	logInfo.Printf(msg, a...)
}

func main() {
	// read json config filename from command line arguments
	var fnameConfig string
	flag.StringVar(&fnameConfig, "config", "", "configuration file")
	flag.Parse()

	// make sure filename was specified
	if len(fnameConfig) == 0 {
		flag.Usage()
		return
	}

	// seed random number generator
	rand.Seed(time.Now().UTC().UnixNano())

	// open config file
	fConfig, err := ioutil.ReadFile(fnameConfig)
	if err != nil {
		panic(fmt.Sprintf("could not read config file '%s'", fnameConfig))
	}

	// parse json config
	var config Config
	if err = json.Unmarshal(fConfig, &config); err != nil {
		panic(fmt.Sprintf("could not parse config file '%s'", fnameConfig))
	}

	// create actions
	for i, configAction := range config.Actions {
		action := Action{
			id:         i,
			ippBase:    configAction.IPPBase,
			ippPayload: configAction.IPPPayload,
			share:      configAction.Share,
		}
		actions = append(actions, action)
	}

	// iterate over trace configuration
	for i, configTrace := range config.Traces {
		printLog(0, "Trace %d/%d ...", i+1, len(config.Traces))

		// load flows from trace
		printLog(1, "loading flows from trace trace ...")
		loadFlowsFromTrace(configTrace.FnamePCAP)

		// assign actions
		printLog(1, "assigning actions %d times ...", config.NRuns)
		successfulAssignments := 0
		for successfulAssignments < config.NRuns {
			if assignActions(config.MaxErr) {
				successfulAssignments++
				printLog(2, "... assignment %d successful", successfulAssignments)
			}
		}

		printLog(1, "exporting ipps ...")
		exportInstrPerPacket(configTrace.FnamePCAP, configTrace.FnameIPP, config.NRuns)

		printLog(1, "exporting dimensioning ...")
		exportDimensioning(configTrace.FnameDimensioning, config.NRuns)

		printLog(1, "exporting infos ...")
		exportInfos(configTrace.FnameInfo, config.NRuns)
	}
}

func getPktPayloadLen(pkt gopacket.Packet) int {
	// get network layer
	netlayer := pkt.NetworkLayer()
	if netlayer == nil {
		panic("trace packet does not have network layer")
	}

	var nBytesPayload uint16

	if netlayer.LayerType() == layers.LayerTypeIPv4 {
		// IPv4
		ip4 := pkt.Layer(layers.LayerTypeIPv4).(*layers.IPv4)

		// get payload length
		nBytesPayload = ip4.Length - uint16(ip4.IHL*4)
	} else if netlayer.LayerType() == layers.LayerTypeIPv6 {
		// IPv6
		ip6 := pkt.Layer(layers.LayerTypeIPv6).(*layers.IPv6)

		// get payload length
		nBytesPayload = ip6.Length
	} else {
		panic("trace packet is non-ip")
	}

	return int(nBytesPayload)
}

func loadFlowsFromTrace(fnamePCAP string) {
	// open the trace
	trace := gonettrace.TraceOpen(fnamePCAP, layers.LinkTypeRaw)

	// initialize flow map
	flowMap = make(map[string]*FlowData)

	firstPacket := true
	var timestamp, timestampFirst time.Time

	// iterate over packets in the trace
	for pkt := range trace.Packets() {
		// get packet's payload length
		nBytesPayload := getPktPayloadLen(pkt)

		// some error checking
		if int(nBytesPayload) >= pkt.Metadata().Length {
			panic("invalid packet length")
		}

		// create flow from packet
		flow := gonettrace.FlowCreate(pkt)

		// get flow key for lookup in map
		key := flow.Key()

		// get data for this flow from map
		data, ok := flowMap[key]

		if !ok {
			// if data for this flow is not in the map already, create new
			// struct
			data = &FlowData{}
		}

		// increment counters
		data.nPkts++
		data.nBytes += uint64(pkt.Metadata().Length)
		data.nBytesPayload += uint64(nBytesPayload)

		// save data back to map
		flowMap[key] = data

		// save packet tiemstamp
		timestamp = pkt.Metadata().Timestamp

		if firstPacket {
			timestampFirst = timestamp
			firstPacket = false
		}
	}

	// convert flow map to flow list
	flows = make([]*FlowData, len(flowMap))
	i := 0
	for _, flow := range flowMap {
		flows[i] = flow
		i++
	}

	// save trace duration
	traceDuration = timestamp.Sub(timestampFirst)
}

func assignActions(maxErr float64) bool {
	// make sure that the assignment shares add up to one
	sharesTotal := 0.0
	for _, action := range actions {
		sharesTotal += action.share
	}
	if math.Abs(1.0-sharesTotal) > 0.000001 { // hard-coded epsilon. not pretty but works for now
		panic("assignment share does not add up to 1.0")
	}

	// get the total number of bytes
	nBytesTotal := uint64(0)
	for _, flow := range flows {
		nBytesTotal += flow.nBytes
	}

	// randomly shuffle flows (c.f. fisher-yates shuffle)
	for i := len(flows) - 1; i > 0; i-- {
		j := rand.Intn(i + 1)
		flows[i], flows[j] = flows[j], flows[i]
	}

	// initialize a list, which will contain the number of bytes assigned to
	// each action
	nBytesAssigned := make([]uint64, len(actions))

	// initialize a list that will hold the actions that have been assigned
	// to each flow. the action is not written to the flow immediately,
	// because it must be checked first whether the assignment was successful
	// after all actions for each flow have been determined. if the check is
	// successful, the actions stored in this list are later written to the
	// flows
	assignedActions := make([]Action, len(flows))

	// loop over all flows and assign actions to them
	for i, flow := range flows {
		// target action id
		var actionID int

		if i == 0 {
			// this is the first flow, assign action id 0
			actionID = 0
		} else {
			// which action needs most bytes to reach its target share?
			maxCurrentErr := 0.0
			for j, action := range actions {
				// what's the current share of bytes assigned to the action?
				currentShare := float64(nBytesAssigned[j]) / float64(nBytesTotal)

				// what's the error to the target share?
				currentErr := action.share - currentShare

				// new maximum error?
				if currentErr > maxCurrentErr {
					maxCurrentErr = currentErr
					actionID = j
				}
			}
		}

		// increment number of bytes assigned to this action
		nBytesAssigned[actionID] += flow.nBytes

		// save action assigned for this flow
		assignedActions[i] = actions[actionID]
	}

	// make sure all bytes have been assigned
	nBytesAssignedTotal := uint64(0)
	for _, n := range nBytesAssigned {
		nBytesAssignedTotal += n
	}
	if nBytesAssignedTotal != nBytesTotal {
		panic("actions have not been assigned to all traffic")
	}

	// make sure assignment is within error margin
	assignmentOkay := true
	for i, action := range actions {
		// calculate share of assigned bytes
		shareAssignedBytes := float64(nBytesAssigned[i]) / float64(nBytesTotal)

		// abort if not within margins
		if shareAssignedBytes < (action.share - maxErr) {
			assignmentOkay = false
			break
		} else if shareAssignedBytes > (action.share + maxErr) {
			assignmentOkay = false
			break
		}
	}

	// write actions to flow meta data if assignment was successful
	if assignmentOkay {
		for i, action := range assignedActions {
			flows[i].actions = append(flows[i].actions, action)
		}
	}

	// return whether assignment was successful or not
	return assignmentOkay
}

func exportInstrPerPacket(fnamePCAP, fnameOut string, nRuns int) {
	f := make([]*os.File, nRuns)
	for i := 0; i < nRuns; i++ {
		// replace '${r}' by actual run number in output filename
		fnameOutRun := strings.Replace(fnameOut, "${r}", strconv.Itoa(i), -1)

		// create output file for this run
		var err error
		f[i], err = os.Create(fnameOutRun)
		if err != nil {
			panic(err)
		}
	}

	// open the trace
	trace := gonettrace.TraceOpen(fnamePCAP, layers.LinkTypeRaw)

	// iterate over all packets in the trace
	for pkt := range trace.Packets() {
		// create flow from packet
		flow := gonettrace.FlowCreate(pkt)

		// get flow key for lookup in map
		key := flow.Key()

		// get flow data from map
		data, ok := flowMap[key]

		// flow data MUST be in map
		if !ok {
			panic("flow lookup failed")
		}

		// get packet's payload length
		nBytesPayload := getPktPayloadLen(pkt)

		for i := 0; i < nRuns; i++ {
			// get action executed in this run
			action := data.actions[i]

			// calculate instructions to execute for this packet
			instr := action.ippBase
			instr += nBytesPayload * action.ippPayload

			// write number of instructions to file
			fmt.Fprintf(f[i], "%d\n", instr)
		}
	}

	// close files
	for _, file := range f {
		file.Close()
	}
}

func exportDimensioning(filenameOut string, nRuns int) {
	// create output file
	f, err := os.Create(filenameOut)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	for i := 0; i < nRuns; i++ {
		var instrTotal uint64

		// iterate over all flows
		for _, flow := range flows {
			// get action
			action := flow.actions[i]

			// add up executed instructions
			instrTotal += uint64(flow.nPkts) * uint64(action.ippBase)
			instrTotal += flow.nBytesPayload * uint64(action.ippPayload)
		}

		// calculate required mean capacity
		capacity := float64(instrTotal) / traceDuration.Seconds()

		// write required mean capacity to file
		fmt.Fprintf(f, "%f\n", capacity)
	}
}

func exportInfos(filenameOut string, nRuns int) {
	// create output file
	f, err := os.Create(filenameOut)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	for i := 0; i < nRuns; i++ {
		nBytesPerAction := make([]uint64, len(actions))

		// iterate over all flows
		for _, flow := range flows {
			nBytesPerAction[flow.actions[i].id] += flow.nBytes
		}

		fmt.Fprintf(f, "n_bytes_per_action,%d,", i)

		for j, nBytes := range nBytesPerAction {
			fmt.Fprintf(f, "%d", nBytes)

			if j == len(nBytesPerAction)-1 {
				fmt.Fprint(f, "\n")
			} else {
				fmt.Fprint(f, ",")
			}
		}
	}
}
