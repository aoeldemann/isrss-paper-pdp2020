#include "Packet.h"
#include "PacketNodeContext.h"

Packet::Packet(const char *name) : Packet_Base(name)
{
  m_id = -1;
  m_flow = NULL;
  m_hop_cnt = 0;
  m_node_ctx = new PacketNodeContext;
  m_instr = 0;
  m_processing_done = false;
}

Packet::Packet(const Packet &other) : Packet_Base(other) { operator=(other); }

Packet::~Packet() { delete m_node_ctx; }

Packet &Packet::operator=(const Packet &other)
{
  if (&other == this) {
    return *this;
  }
  Packet_Base::operator=(other);
  m_id = other.m_id;
  m_flow = other.m_flow;
  m_hop_cnt = other.m_hop_cnt;
  m_node_ctx = other.m_node_ctx;
  m_instr = other.m_instr;
  m_processing_done = other.m_processing_done;
  return *this;
}

Packet *Packet::dup() const { return new Packet(*this); }

void Packet::set_id(uint64_t id)
{
  ASSERT(m_id == -1);
  m_id = (int64_t)id;
}

uint64_t Packet::get_id()
{
  ASSERT(m_id != -1);
  return (uint64_t)m_id;
}

void Packet::set_flow(Flow *flow)
{
  ASSERT(m_flow == NULL);
  m_flow = flow;
}

Flow *Packet::get_flow()
{
  ASSERT(m_flow);
  return m_flow;
}

Latency *Packet::get_latency() { return &m_latency; }

void Packet::set_hop_cnt(uint8_t hop_cnt) { m_hop_cnt = hop_cnt; }

uint8_t Packet::get_hop_cnt() { return m_hop_cnt; }

void Packet::incrm_hop_cnt() { m_hop_cnt++; }

PacketNodeContext *Packet::get_node_ctx() { return m_node_ctx; }

void Packet::set_instr(uint32_t instr) { m_instr = instr; }

uint32_t Packet::get_instr() { return m_instr; }

void Packet::set_processing_done()
{
  ASSERT(!m_processing_done);
  m_processing_done = true;
}

bool Packet::is_processing_done() { return m_processing_done; }
