#ifndef MODULES_NODE_PROCESSING_H_
#define MODULES_NODE_PROCESSING_H_

#include <omnetpp.h>

using namespace omnetpp;

class OffloadTrigger;
class Packet;

class Processing : public cSimpleModule
{
public:
  virtual ~Processing();

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);
  virtual Packet *process_packet(uint8_t core_id);

  uint8_t m_n_cores;

private:
  typedef struct {
    cPacketQueue rx_queue; // packet rx queue assigned to this core
    bool busy;             // core busy?
    // message scheduled when processing of packet is done
    cMessage *msg_proc_done;
  } core_t;

  void set_t_inst(uint8_t core_id, simtime_t t_inst);
  void send_pkt(Packet *pkt);
  void set_busy(uint8_t core_id, bool busy);
  bool is_busy(uint8_t core_id);
  uint32_t get_queue_len(uint8_t core_id);

  simtime_t *m_t_inst;

  std::vector<core_t> m_cores;
  uint8_t m_n_cores_busy;

  OffloadTrigger *m_module_offload_trigger;

  simsignal_t m_sig_stats_ipp;
  simsignal_t m_sig_stats_proc_util;
  simsignal_t *m_sigs_stats_proc_util_core;
  simsignal_t *m_sigs_stats_queue_len;
};

#endif
