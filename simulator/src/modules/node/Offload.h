#ifndef MODULES_NODE_OFFLOAD_H_
#define MODULES_NODE_OFFLOAD_H_

#include <omnetpp.h>

using namespace omnetpp;

class Packet;
class OffloadTriggerMsg;

class Offload : public cSimpleModule
{
public:
  virtual ~Offload();

  // TODO: implement by exchanging messages between processing and offloading
  // modules
  void update_rss_reta_entry(uint16_t entry, uint8_t rx_queue);

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);

private:
  typedef struct {
    bool valid;
    uint8_t local_rx_queue;
    bool offload;
    simtime_t t_last_arrival;
  } hashtable_entry_t;

  void handle_pkt(Packet *pkt);
  void handle_offload_trigger(OffloadTriggerMsg *msg);
  void send_pkt_local(Packet *pkt, uint8_t rx_queue);
  void send_pkt_offload(Packet *pkt, int32_t port);
  int8_t calc_local_rx_queue_not_overloaded(Packet *pkt);
  uint32_t calc_rss_rx_queue(Packet *pkt);

  hashtable_entry_t *m_hashtable;

  bool m_enabled_balance_cores;
  bool m_enabled_offload;
  uint32_t m_hashtable_size;
  simtime_t m_hashtable_entry_timeout;
  uint8_t m_max_hop_cnt;
  uint8_t m_n_rx_queues;
  bool *m_rx_queue_overload;

  uint16_t m_rss_reta_size;
  uint8_t *m_rss_reta;
};

#endif
