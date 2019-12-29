#ifndef MODULES_TORSWITCH_H_
#define MODULES_TORSWITCH_H_

#include <omnetpp.h>

using namespace omnetpp;

class Packet;

class TorSwitch : public cSimpleModule
{
public:
  virtual ~TorSwitch();

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);

  uint8_t m_n_ports_nodes;

private:
  void handle_packet(Packet *pkt);
  void send_packet_to_node(cPacket *pkt, uint8_t port_id);
  void send_packet_to_sink(cPacket *pkt, uint8_t port_id);
  void send_packet_from_buffer_to_node(uint8_t port_id);
  uint8_t select_output_port(Packet *pkt);

  cPacketQueue *m_queues;
  cChannel **m_channels_nodes;
  cMessage *m_self_msgs;
};

#endif
