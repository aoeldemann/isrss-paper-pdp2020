#include "Sink.h"
#include "../defines.h"
#include "../msgs/Packet.h"

Define_Module(Sink);

Sink::~Sink()
{
  if (m_enable_reorder_check) {
    // delete reorder check table
    for (int i = 0; i < CHECK_HASHTABLE_ENTRIES; i++) {
      m_reorder_check_table[i].clear();
    }
    delete[] m_reorder_check_table;
  }
}

void Sink::initialize()
{
  // register statistic singals
  m_stats_sig_n_packets = registerSignal("stats_n_packets");
  m_stats_sig_hop_cnt = registerSignal("stats_hop_cnt");

  // set histogram ranges and number of bins
  m_stats_hist_lat_end_to_end.setRange(0.0, 10.0);
  m_stats_hist_lat_end_to_end.setNumBinsHint(10000000);

  m_stats_hist_lat_node_buffer_in.setRange(0.0, 10.0);
  m_stats_hist_lat_node_buffer_in.setNumBinsHint(10000000);

  m_stats_hist_lat_node_buffer_out.setRange(0.0, 1.0);
  m_stats_hist_lat_node_buffer_out.setNumBinsHint(1000000);

  m_stats_hist_lat_node_proc.setRange(0.0, 0.001);
  m_stats_hist_lat_node_proc.setNumBinsHint(1000);

  m_stats_hist_lat_tor.setRange(0.0, 1.0);
  m_stats_hist_lat_tor.setNumBinsHint(1000000);

  m_stats_lat_end_to_end = registerSignal("stats_lat_end_to_end");
  m_stats_lat_node_buffer_in = registerSignal("stats_lat_node_buffer_in");
  m_stats_lat_node_buffer_out = registerSignal("stats_lat_node_buffer_out");
  m_stats_lat_node_proc = registerSignal("stats_lat_node_proc");
  m_stats_lat_tor = registerSignal("stats_lat_tor");

  // reorder checking enabled?
  m_enable_reorder_check = par("check_reorder");

  if (m_enable_reorder_check) {
    // initialize reorder check hashtable
    m_reorder_check_table =
        new std::vector<reorder_check_table_entry_t>[CHECK_HASHTABLE_ENTRIES];

    // register statistic signals
    m_stats_sig_reorder_check_n_reordered_pkts =
        registerSignal("stats_reorder_check_n_reordered_pkts");
    m_stats_sig_reorder_check_n_reordered_flows =
        registerSignal("stats_reorder_check_n_reordered_flows");
  }
}

void Sink::finish()
{
  // records cdfs
  record_cdf_simtime("lat_end_to_end_cdf", 1000, &m_stats_hist_lat_end_to_end);
  record_cdf_simtime("lat_node_buffer_in", 1000,
                     &m_stats_hist_lat_node_buffer_in);
  record_cdf_simtime("lat_node_buffer_out", 1000,
                     &m_stats_hist_lat_node_buffer_out);
  record_cdf_simtime("lat_node_proc", 1000, &m_stats_hist_lat_node_proc);
  record_cdf_simtime("lat_tor", 1000, &m_stats_hist_lat_tor);
}

void Sink::handleMessage(cMessage *msg)
{
  // only data packets must arrive here!
  ASSERT(msg->getKind() == MSG_KIND_PACKET_DATA);

  // increment packet counter
  emit(m_stats_sig_n_packets, 1);

  // cast packet
  Packet *pkt = (Packet *)msg;

  // make sure packet has been processed
  ASSERT(pkt->is_processing_done());

  // get latency object
  Latency *latency = pkt->get_latency();

  // get latency value
  simtime_t lat_node_buffer_in =
      latency->get_total_latency_by_type(LatencyElement::NODE_BUFFER_IN);
  simtime_t lat_node_buffer_out =
      latency->get_total_latency_by_type(LatencyElement::NODE_BUFFER_OUT);
  simtime_t lat_node_proc =
      latency->get_total_latency_by_type(LatencyElement::NODE_PROC);
  simtime_t lat_tor = latency->get_total_latency_by_type(LatencyElement::TOR);

  // get end-to-end packet latency
  simtime_t lat_end_to_end = pkt->get_latency()->get_end_to_end_latency();

  // collect latency values
  m_stats_hist_lat_end_to_end.collect(lat_end_to_end);
  m_stats_hist_lat_node_buffer_in.collect(lat_node_buffer_in);
  m_stats_hist_lat_node_buffer_out.collect(lat_node_buffer_out);
  m_stats_hist_lat_node_proc.collect(lat_node_proc);
  m_stats_hist_lat_tor.collect(lat_tor);

  // collect hop count
  emit(m_stats_sig_hop_cnt, pkt->get_hop_cnt());

  // report end-to-end packet latency
  emit(m_stats_lat_end_to_end, lat_end_to_end);

  // report buffer latencies
  emit(m_stats_lat_node_buffer_in, lat_node_buffer_in);
  emit(m_stats_lat_node_buffer_out, lat_node_buffer_out);
  emit(m_stats_lat_node_proc, lat_node_proc);
  emit(m_stats_lat_tor, lat_tor);

  // do reorder check, if necessary
  if (m_enable_reorder_check) {
    // get flow
    Flow *flow = pkt->get_flow();

    // get packet id
    uint64_t pkt_id = pkt->get_id();

    reorder_check(flow, pkt_id);
  }

  delete pkt;
}

void Sink::reorder_check(Flow *flow, uint64_t pkt_id)
{
  // see if entry for this flow exists in the table
  reorder_check_table_entry_t *entry = reorder_check_find(flow);

  if (entry) {
    if ((*entry).nxt_exptected_pkt_id > pkt_id) {
      // reordering!
      entry->reorder_cntr++;
      emit(m_stats_sig_reorder_check_n_reordered_pkts, 1);
      if (entry->reorder_cntr == 1) {
        emit(m_stats_sig_reorder_check_n_reordered_flows, 1);
      }
    } else {
      entry->nxt_exptected_pkt_id = pkt_id + 1;
    }
  } else {
    // entry does no exist yet. create it
    reorder_check_table_entry_t entry;
    entry.flow = flow;
    entry.nxt_exptected_pkt_id = pkt_id + 1;
    entry.reorder_cntr = 0;

    // add entry to table
    m_reorder_check_table[flow->get_toeplitz_hash() % CHECK_HASHTABLE_ENTRIES]
        .push_back(entry);
  }
}

Sink::reorder_check_table_entry_t *Sink::reorder_check_find(Flow *flow)
{
  // get toeplitz hash value
  uint32_t hash = flow->get_toeplitz_hash();

  // iterate over reorder check table
  std::vector<reorder_check_table_entry_t>::iterator it =
      m_reorder_check_table[hash % CHECK_HASHTABLE_ENTRIES].begin();
  for (; it != m_reorder_check_table[hash % CHECK_HASHTABLE_ENTRIES].end();
       it++) {
    if ((*it).flow == flow) {
      // found the entry! return pointer to it
      return &(*it);
    }
  }

  // no entry found, return null
  return NULL;
}

void Sink::record_cdf_simtime(const char *scalar_name, uint16_t n_steps,
                              cHistogram *hist)
{
  if (n_steps == 0) {
    return;
  }

  double step_size = 1.0 / n_steps;

  simtime_t cdf_values[n_steps];
  uint64_t n_packets = hist->getCount();
  uint64_t n_packets_acc = 0;
  uint16_t cur_step = 0;

  for (int32_t bin = 0; bin < hist->getNumBins(); bin++) {
    n_packets_acc += (uint64_t)hist->getBinValue(bin);
    double s = (double)n_packets_acc / (double)n_packets;

    while (s >= (cur_step + 1) * step_size) {
      cdf_values[cur_step] =
          hist->getBinEdge(bin) +
          (hist->getBinEdge(bin + 1) - hist->getBinEdge(bin)) / 2.0;
      cur_step++;
    }
  }

  char scalar[64];
  for (uint16_t i = 0; i < n_steps; i++) {
    sprintf(scalar, "%s:%lf", scalar_name, (i + 1) * step_size);
    recordScalar(scalar, cdf_values[i]);
  }
}
