#ifndef MSGS_PACKET_H_
#define MSGS_PACKET_H_

#include "Flow.h"
#include "Latency.h"
#include "Packet_m.h"

class PacketNodeContext;

class Packet : public Packet_Base
{
public:
  Packet(const char *name = NULL);
  Packet(const Packet &other);
  virtual ~Packet();

  Packet &operator=(const Packet &other);
  virtual Packet *dup() const;

  void set_id(uint64_t id);
  uint64_t get_id();

  void set_flow(Flow *flow);
  Flow *get_flow();
  Latency *get_latency();

  void set_hop_cnt(uint8_t hop_cnt);
  uint8_t get_hop_cnt();
  void incrm_hop_cnt();

  PacketNodeContext *get_node_ctx();

  void set_instr(uint32_t instr);
  uint32_t get_instr();

  void set_processing_done();
  bool is_processing_done();

private:
  int64_t m_id;
  Flow *m_flow;
  Latency m_latency;
  uint8_t m_hop_cnt;
  PacketNodeContext *m_node_ctx;
  uint32_t m_instr;
  bool m_processing_done;
};

Register_Class(Packet);

#endif
