// Tool reads a PCAP trace and identifies all unique network flows based on
// IP and TCP/UDP header fields. It assigns a unique integer ID to all unique
// flows. It additionally assigns a (per-flow) unique ID to all packets
// belonging to a trace. For each packet in the trace it prints a
// "<flow_id>:<pkt_id>" to stdout.
package main

import (
	"flag"
	"fmt"

	"github.com/aoeldemann/gonettrace"
	"github.com/google/gopacket/layers"
)

type FlowData struct {
	flowID  uint64
	pktCntr uint64
}

func main() {
	// parse command-line argument
	var fnamePCAP string
	flag.StringVar(&fnamePCAP, "pcap", "", "pcap file")
	flag.Parse()

	// command-line argument specified?
	if len(fnamePCAP) == 0 {
		flag.Usage()
		return
	}

	// open trace
	trace := gonettrace.TraceOpen(fnamePCAP, layers.LinkTypeRaw)

	// create a map to store flow data for all flows in the trace
	flowData := make(map[string]*FlowData)

	// init flow counter
	flowCntr := uint64(0)

	// iterate over packets
	for pkt := range trace.Packets() {
		// create flow from packet
		flow := gonettrace.FlowCreate(pkt)

		// get flow's string key
		key := flow.Key()

		// add flow to map, if its not in it already
		if _, ok := flowData[key]; !ok {
			flowData[key] = &FlowData{flowID: flowCntr}

			// increment flow counter
			flowCntr++
		}

		// print out <flow_id>:<pkt_id> tuple
		fmt.Printf("%d:%d\n", flowData[key].flowID, flowData[key].pktCntr)

		// increment packet counter
		flowData[key].pktCntr++
	}
}
