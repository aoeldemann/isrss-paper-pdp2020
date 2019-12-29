#ifndef MSGS_PACKETNODECONTEXT_H_
#define MSGS_PACKETNODECONTEXT_H_

class PacketNodeContext
{
public:
  PacketNodeContext() { clear(); }

  uint8_t get_rx_queue()
  {
    ASSERT(m_rx_queue != -1);
    return (uint8_t)m_rx_queue;
  }

  void set_rx_queue(uint8_t rx_queue)
  {
    ASSERT(m_rx_queue == -1);
    m_rx_queue = (int16_t)rx_queue;
  }

  bool is_rx_queue_set() { return m_rx_queue != -1; }

  uint8_t get_arrival_port_id()
  {
    ASSERT(m_arrival_port_id != -1);
    return (uint8_t)m_arrival_port_id;
  }

  void set_arrival_port_id(uint8_t arrival_port_id)
  {
    ASSERT(m_arrival_port_id == -1);
    m_arrival_port_id = (int8_t)arrival_port_id;
  }

  void clear()
  {
    m_rx_queue = -1;
    m_arrival_port_id = -1;
  }

private:
  int16_t m_rx_queue;
  int8_t m_arrival_port_id;
};

#endif
