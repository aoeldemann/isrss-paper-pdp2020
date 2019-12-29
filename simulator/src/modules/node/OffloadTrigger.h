#ifndef MODULES_NODE_OFFLOADTRIGGER_H_
#define MODULES_NODE_OFFLOADTRIGGER_H_

#include <omnetpp.h>

using namespace omnetpp;

class Packet;

class OffloadTrigger : public cSimpleModule
{
public:
  virtual ~OffloadTrigger();
  void report_queue_len(uint8_t queue_id, uint32_t queue_len);

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);

  bool is_offload_enabled(uint8_t queue_id);
  void set_offload_enable(uint8_t queue_id, bool enable);

  bool m_enabled;
  uint32_t m_threshold;
  uint8_t m_n_rx_queues;

  bool *m_offload_enabled;
};

#endif
