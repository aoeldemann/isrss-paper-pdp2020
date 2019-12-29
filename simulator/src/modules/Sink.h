#ifndef MODULES_SINK_H_
#define MODULES_SINK_H_

#include <omnetpp.h>

using namespace omnetpp;

#define CHECK_HASHTABLE_ENTRIES 65536

struct Flow;

class Sink : public cSimpleModule
{
public:
  virtual ~Sink();

protected:
  virtual void initialize();
  virtual void finish();
  virtual void handleMessage(cMessage *msg);

private:
  typedef struct {
    Flow *flow;
    uint64_t nxt_exptected_pkt_id;
    uint64_t reorder_cntr;
  } reorder_check_table_entry_t;

  void reorder_check(Flow *flow, uint64_t pkt_id);
  reorder_check_table_entry_t *reorder_check_find(Flow *flow);

  void record_cdf_simtime(const char *scalar_name, uint16_t n_steps,
                          cHistogram *hist);

  simsignal_t m_stats_sig_n_packets;
  simsignal_t m_stats_sig_hop_cnt;

  cHistogram m_stats_hist_lat_end_to_end;
  cHistogram m_stats_hist_lat_node_buffer_in;
  cHistogram m_stats_hist_lat_node_buffer_out;
  cHistogram m_stats_hist_lat_node_proc;
  cHistogram m_stats_hist_lat_tor;

  simsignal_t m_stats_lat_end_to_end;
  simsignal_t m_stats_lat_node_buffer_in;
  simsignal_t m_stats_lat_node_buffer_out;
  simsignal_t m_stats_lat_node_proc;
  simsignal_t m_stats_lat_tor;

  bool m_enable_reorder_check;
  std::vector<reorder_check_table_entry_t> *m_reorder_check_table;

  simsignal_t m_stats_sig_reorder_check_n_reordered_pkts;
  simsignal_t m_stats_sig_reorder_check_n_reordered_flows;
};

#endif
