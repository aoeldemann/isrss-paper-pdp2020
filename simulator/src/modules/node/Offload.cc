#include "Offload.h"
#include "../../defines.h"
#include "../../msgs/OffloadTriggerMsg_m.h"
#include "../../msgs/Packet.h"
#include "../../msgs/PacketNodeContext.h"

Define_Module(Offload);

Offload::~Offload()
{
  if (m_enabled_balance_cores || m_enabled_offload) {
    delete[] m_hashtable;
    delete[] m_rx_queue_overload;
  }
  delete[] m_rss_reta;
}

void Offload::initialize()
{
  // what offloading is enabled?
  m_enabled_balance_cores = par("enable_balance_cores");
  m_enabled_offload = par("enable_offload");

  // get hashtable size
  m_hashtable_size = par("hashtable_size");
  ASSERT(m_hashtable_size > 0);

  // get max hop count paramter
  m_max_hop_cnt = par("max_hop_cnt");
  ASSERT(m_max_hop_cnt > 0);

  // number of input and output gates must both be 1 for now
  ASSERT(gateSize("in") == 1);
  ASSERT(gateSize("out") == 1);

  // get number of rx queues
  m_n_rx_queues = par("n_rx_queues");

  // rss reta table has size of offloading hash table
  m_rss_reta_size = m_hashtable_size;

  // initialize rss reta table
  m_rss_reta = new uint8_t[m_rss_reta_size];
  for (uint16_t i = 0; i < m_rss_reta_size; i++) {
    m_rss_reta[i] = i % m_n_rx_queues;
  }

  if (!m_enabled_balance_cores && !m_enabled_offload) {
    // nothing more to do here
    return;
  }

  // initialize other parameter values
  m_hashtable_size = par("hashtable_size");
  m_hashtable_entry_timeout = par("hashtable_entry_timeout");
  ASSERT(m_hashtable_size > 0);

  // initialize hash table. initially mark all entries as inactive (i.e. no
  // packet has hit the entry yet)
  m_hashtable = new hashtable_entry_t[m_hashtable_size];
  for (uint32_t i = 0; i < m_hashtable_size; i++) {
    m_hashtable[i].valid = false;
  }

  // initially no cores attached to the rx queues are overloaded
  m_rx_queue_overload = new bool[m_n_rx_queues];
  for (uint8_t i = 0; i < m_n_rx_queues; i++) {
    m_rx_queue_overload[i] = false;
  }
}

void Offload::handleMessage(cMessage *msg)
{
  // get message kind
  uint16_t msgKind = msg->getKind();

  if (msgKind == MSG_KIND_PACKET_DATA) {
    // this is a data packet
    handle_pkt((Packet *)msg);
  } else if (msgKind == MSG_KIND_OFFLOAD_TRIGGER) {
    // this is an offload trigger message
    handle_offload_trigger((OffloadTriggerMsg *)msg);
  } else {
    ASSERT(false && "invalid message kind");
  }
}

void Offload::handle_pkt(Packet *pkt)
{
  if (m_enabled_offload) {
    // the current hop count of the packet must be smaller than the configured
    // maximum hop count
    ASSERT(pkt->get_hop_cnt() < m_max_hop_cnt);
  } else {
    // no offloading is enabled, so the hop count should always be zero, because
    // the packet should not have traversed any other nodes. hop count is
    // incremented in output buffer
    ASSERT(pkt->get_hop_cnt() == 0);
  }

  if (!m_enabled_balance_cores && !m_enabled_offload) {
    // no offload functionality is enabled. determine target rx queue based on
    // rss reta and send packet to local node. nothing more to do!
    uint8_t rx_queue = calc_rss_rx_queue(pkt);
    send_pkt_local(pkt, rx_queue);
    return;
  }

  // get toeplitz hash
  uint32_t toeplitz_hash = pkt->get_flow()->get_toeplitz_hash();

  // lookup hashtable entry
  hashtable_entry_t &ht_entry = m_hashtable[toeplitz_hash % m_hashtable_size];

  // determine the target rx queue for the case that the arriving packet shall
  // be processed locally.
  uint8_t local_rx_queue;
  if (m_enabled_balance_cores) {
    // local core balancing is enabled. in case the hash table entry is not
    // active yet (i.e. no packet has hit in the past), start out by assigning a
    // local core based on toeplitz hash. this core might be updated in the
    // next steps
    if (ht_entry.valid == false) {
      ht_entry.local_rx_queue = calc_rss_rx_queue(pkt);
    }

    // is the currently assigned core overloaded?
    if (m_rx_queue_overload[ht_entry.local_rx_queue]) {
      // core is overloaded. let's see if there is another core that is
      // available and could take over processing
      int8_t queue = calc_local_rx_queue_not_overloaded(pkt);
      if (queue == -1) {
        // we did not find any cores that are not overloaded. in case the packet
        // will be processed locally, let's process it on the same core that
        // previous packets hitting the hash table entry have been processed
        local_rx_queue = ht_entry.local_rx_queue;
      } else {
        // we found a core that is available. let's process the packet there
        local_rx_queue = (uint8_t)queue;
      }
    } else {
      // core is not overloaded. let's keep processing there
      local_rx_queue = ht_entry.local_rx_queue;
    }
  } else {
    // local core balancing is disabled. if we process the arriving packet
    // locally, do that on the core identified by rss reta
    local_rx_queue = calc_rss_rx_queue(pkt);
  }

  // if remote offloading is enabled and the previously determined core is
  // overloaded, we prefer to offload the packet to another node
  bool offload = m_enabled_offload && m_rx_queue_overload[local_rx_queue];

  // if remote offloading is enabled and the max hop count is reached (we are
  // the last hop in the offloading ring), we must process the packet locally.
  // remember that the hop count is incremented in the egress logic in the
  // output buffer (thus we are comparing against max_hop_cnt - 1).
  bool force_local =
      m_enabled_offload && (pkt->get_hop_cnt() == (m_max_hop_cnt - 1));

  if ((ht_entry.valid == false) ||
      (simTime() >= (ht_entry.t_last_arrival + m_hashtable_entry_timeout))) {
    // the hashtable entry is either hit for the first time or the timeout has
    // expired. in this case, we may actually write the determined local rx
    // queue and the offload decision to the hash table
    ht_entry.offload = offload;
    ht_entry.local_rx_queue = local_rx_queue;
  }
  // mark entry as active and update last arrival time
  ht_entry.valid = true;
  ht_entry.t_last_arrival = simTime();

  if (ht_entry.offload && (force_local == false)) {
    // offload packet if the offload flag in the hash table is set and we are
    // no the last hop in the offloading ring
    send_pkt_offload(pkt, 0);
  } else {
    // otherwise place packet in the rx queue indicated in the hash table
    send_pkt_local(pkt, ht_entry.local_rx_queue);
  }
}

void Offload::handle_offload_trigger(OffloadTriggerMsg *msg)
{
  // when an offload trigger message is received, local core balancing or remote
  // offloading must be enabled
  ASSERT(m_enabled_balance_cores || m_enabled_offload);

  // mark rx queue as overloaded/not overloaded
  m_rx_queue_overload[msg->getQueueId()] = msg->getActive();

  delete msg;
}

void Offload::send_pkt_local(Packet *pkt, uint8_t rx_queue)
{
  // get packet's node context
  PacketNodeContext *ctx = pkt->get_node_ctx();

  // all packets arriving here should not have a target rx queue set
  ASSERT(ctx->is_rx_queue_set() == false);

  // set rx queue
  ctx->set_rx_queue(rx_queue);

  // send packet to local node for processing
  send(pkt, "out_proc");
}

void Offload::send_pkt_offload(Packet *pkt, int32_t port)
{
  // offload packet to another node
  send(pkt, "out", port);
}

int8_t Offload::calc_local_rx_queue_not_overloaded(Packet *pkt)
{
  // initialize empty list, which will hold the rx queues that are served by
  // cores that are not overloaded
  uint8_t selected_queues[m_n_rx_queues];
  uint8_t len_selected_queues = 0;

  // iterate over all rx queues
  for (uint8_t i = 0; i < m_n_rx_queues; i++) {
    // core serving the queue overloaded?
    if (m_rx_queue_overload[i] == false) {
      // no, add it to the list
      selected_queues[len_selected_queues] = i;
      len_selected_queues++;
    }
  }

  if (len_selected_queues == 0) {
    // unfortunately, we did not find any cores that are not overloaded
    return -1;
  } else {
    // found at least one core that is not overloaded. select an entry from the
    // list based on toeplitz hash
    return selected_queues[pkt->get_flow()->get_toeplitz_hash() %
                           len_selected_queues];
  }
}

uint32_t Offload::calc_rss_rx_queue(Packet *pkt)
{
  // determine and return target rx queue id based on rss reta
  return m_rss_reta[pkt->get_flow()->get_toeplitz_hash() % m_rss_reta_size];
}

void Offload::update_rss_reta_entry(uint16_t entry, uint8_t rx_queue)
{
  // update rss reta entry
  ASSERT(entry < m_rss_reta_size);
  m_rss_reta[entry] = rx_queue;
}
