#include "Processing.h"
#include "../../defines.h"
#include "../../msgs/Packet.h"
#include "../../msgs/PacketNodeContext.h"
#include "OffloadTrigger.h"

Define_Module(Processing)

    Processing::~Processing()
{
  // cancel any events that may still be active
  for (uint8_t i = 0; i < m_n_cores; i++) {
    // get core struct
    core_t &core = m_cores[i];

    // if there is currently a packet being processed on the core, delete it
    if (core.msg_proc_done->isScheduled()) {
      delete (Packet *)core.msg_proc_done->getContextPointer();
    }

    // cancel possibly outstanding self-message
    cancelAndDelete(core.msg_proc_done);
  }

  delete[] m_t_inst;
  delete[] m_sigs_stats_proc_util_core;
  delete[] m_sigs_stats_queue_len;
}

void Processing::initialize()
{
  // get the number of CPU cores
  m_n_cores = par("n_cores");

  // get the per CPU core processing capacity (instructions/seconds)
  double capacity_per_core = par("capacity_per_core");

  // calculate the duration of one CPU instruction
  simtime_t t_inst = 1.0 / capacity_per_core;

  // initialize all CPU cores with the same capacity
  m_t_inst = new simtime_t[m_n_cores];
  for (uint8_t i = 0; i < m_n_cores; i++) {
    set_t_inst(i, t_inst);
  }

  // record total capacity
  recordScalar("capacity_total", m_n_cores * capacity_per_core);

  // register signals for stats collection (executed IPP and processor
  // utilization)
  m_sig_stats_ipp = registerSignal("stats_ipp");
  m_sig_stats_proc_util = registerSignal("stats_proc_util");

  // register signals for stats collection (per-core utilization, queue lengths)
  // and create core_t struct including self-messages for per-core event
  // notification
  m_sigs_stats_proc_util_core = new simsignal_t[m_n_cores];
  m_sigs_stats_queue_len = new simsignal_t[m_n_cores];
  for (uint8_t i = 0; i < m_n_cores; i++) {
    core_t core;
    core.busy = false; // initially core not busy
    // create self-message triggered when a packet is complete processed and
    // set its type
    core.msg_proc_done = new cMessage();
    core.msg_proc_done->setKind(MSG_KIND_PROC_DONE);
    m_cores.push_back(core);

    // per-core utilization
    char signal_name[32];
    char stats_name[32];
    sprintf(signal_name, "stats_proc_util_core%d", i);
    sprintf(stats_name, "proc_util_core%d", i);
    m_sigs_stats_proc_util_core[i] = registerSignal(signal_name);
    cProperty *statisticsTemplate =
        getProperties()->get("statisticTemplate", "proc_util_core");
    getEnvir()->addResultRecorders(this, m_sigs_stats_proc_util_core[i],
                                   stats_name, statisticsTemplate);

    // per-core queue length
    sprintf(signal_name, "stats_queue_len%d", i);
    sprintf(stats_name, "queue_len%d", i);
    m_sigs_stats_queue_len[i] = registerSignal(signal_name);
    statisticsTemplate = getProperties()->get("statisticTemplate", "queue_len");
    getEnvir()->addResultRecorders(this, m_sigs_stats_queue_len[i], stats_name,
                                   statisticsTemplate);
  }

  // initially no core is busy
  m_n_cores_busy = 0;

  // get pointer on offload trigger module
  m_module_offload_trigger =
      (OffloadTrigger *)getModuleByPath("^.offload_trigger");
  ASSERT(m_module_offload_trigger);
}

void Processing::handleMessage(cMessage *msg)
{
  if (msg->isSelfMessage() == false) {
    // new packet arriving
    ASSERT(msg->getKind() == MSG_KIND_PACKET_DATA);
    Packet *pkt = (Packet *)msg;

    // get node context
    PacketNodeContext *ctx = pkt->get_node_ctx();

    // obtain target rx queue/core id from node context
    uint8_t core_id = ctx->get_rx_queue();

    // insert packet into correct rx queue
    m_cores[core_id].rx_queue.insert(pkt);

    // emit queue length statistics
    emit(m_sigs_stats_queue_len[core_id],
         m_cores[core_id].rx_queue.getLength());

    if (is_busy(core_id) == false) {
      // target core has been idle. set it active now and start processing the
      // packet we just received
      set_busy(core_id, true);
      process_packet(core_id);
    }
  } else {
    // this is a self-message signaling that a packet has been completely
    // processed

    // make sure its really a processing done message
    ASSERT(msg->getKind() == MSG_KIND_PROC_DONE);

    // get pointer on the packet that has been processed from the context of the
    // self-message
    Packet *pkt = (Packet *)msg->getContextPointer();

    // get packet's node context
    PacketNodeContext *ctx = pkt->get_node_ctx();

    // obtain id of the core where the packet has been processed
    uint8_t core_id = ctx->get_rx_queue();

    // get core struct
    core_t &core = m_cores[core_id];

    // do some error checking
    ASSERT(msg == core.msg_proc_done);
    ASSERT(is_busy(core_id));

    // send out the packet
    send_pkt(pkt);

    if (core.rx_queue.isEmpty() == false) {
      // there are more packets waiting to be processed on this core. trigger
      // processing of next one
      process_packet(core_id);
    } else {
      // no more packets waiting to be processed on this core. set core idle
      set_busy(core_id, false);
    }
  }
}

Packet *Processing::process_packet(uint8_t core_id)
{
  ASSERT(is_busy(core_id));

  // get core
  core_t &core = m_cores[core_id];

  // report queue length
  m_module_offload_trigger->report_queue_len(core_id, get_queue_len(core_id));

  // get rx queue
  cPacketQueue &rx_queue = core.rx_queue;
  ASSERT(rx_queue.isEmpty() == false);

  // pop packet from rx queue
  Packet *pkt = (Packet *)rx_queue.pop();

  // get number of instructions to execute on this packet
  uint32_t instr = pkt->get_instr();
  ASSERT(instr > 0);

  // calculate the time required to execute the IPPs
  simtime_t t_proc = instr * m_t_inst[core_id];

  // report IPP statistics
  emit(m_sig_stats_ipp, instr);

  // calculate the duration that the packet has been waiting in the input
  // buffer
  simtime_t t_buffer = simTime() - pkt->getArrivalTime();

  // get packet's latency object
  Latency *latency = pkt->get_latency();

  // add latency element for the input buffer duration
  LatencyElement *latency_buffer =
      new LatencyElement(LatencyElement::NODE_BUFFER_IN, t_buffer);
  latency->add_element(latency_buffer);

  // add latency element for the processing duration
  LatencyElement *latency_proc =
      new LatencyElement(LatencyElement::NODE_PROC, t_proc);
  latency->add_element(latency_proc);

  // schedule self-message to be sent after processing is completed. pass along
  // a pointer to the packet as context
  core.msg_proc_done->setContextPointer(pkt);
  scheduleAt(simTime() + t_proc, core.msg_proc_done);

  // mark packet as being processed
  pkt->set_processing_done();

  // return the packet that is being processed
  return pkt;
}

void Processing::set_busy(uint8_t core_id, bool busy)
{
  // save core busy/idle state
  m_cores[core_id].busy = busy;

  // maintain number of currently busy cores
  if (busy) {
    m_n_cores_busy++;
    ASSERT(m_n_cores_busy <= m_n_cores);
  } else {
    ASSERT(m_n_cores_busy > 0);
    m_n_cores_busy--;
  }

  // report utilization statistics
  emit(m_sigs_stats_proc_util_core[core_id], busy);
  emit(m_sig_stats_proc_util, m_n_cores_busy);
}

void Processing::send_pkt(Packet *pkt)
{
  // send packet out
  send(pkt, "out");
}

bool Processing::is_busy(uint8_t core_id)
{
  // return whether the specified core is currently busy
  return m_cores[core_id].busy;
}

uint32_t Processing::get_queue_len(uint8_t core_id)
{
  // return the number of packets that are currently waiting to be processed
  // by the specified CPU core.
  return (uint32_t)m_cores[core_id].rx_queue.getLength();
}

void Processing::set_t_inst(uint8_t core_id, simtime_t t_inst)
{
  // that the time the core takes to complete one instruction
  m_t_inst[core_id] = t_inst;
}
