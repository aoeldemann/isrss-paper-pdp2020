#include "TorSwitch.h"
#include "../msgs/Packet.h"

Define_Module(TorSwitch);

TorSwitch::~TorSwitch()
{
  delete[] m_queues;
  delete[] m_channels_nodes;

  // cancel scheduled self-messages
  for (uint8_t i = 0; i < m_n_ports_nodes; i++) {
    cancelEvent(&m_self_msgs[i]);
  }

  delete[] m_self_msgs;
}

void TorSwitch::initialize()
{
  // get number of ports connected to nodes
  m_n_ports_nodes = gateSize("nodes$o");

  // get number of ports connected to sinks
  uint8_t n_ports_sinks = gateSize("sinks");

  // make sure that number of ports connected to nodes is equal to number of
  // ports connected to sinks
  ASSERT(m_n_ports_nodes == n_ports_sinks);

  // create packet queues for ports connected to nodes
  m_queues = new cPacketQueue[m_n_ports_nodes];

  // create array of transmission channels connected to nodes
  m_channels_nodes = new cChannel *[m_n_ports_nodes];

  // create one self-message for each port connected to a node
  m_self_msgs = new cMessage[m_n_ports_nodes];

  for (uint8_t i = 0; i < m_n_ports_nodes; i++) {
    // get transmission channels connected to nodes
    m_channels_nodes[i] = gate("nodes$o", i)->getTransmissionChannel();

    // set self-message's kind to match port id
    m_self_msgs[i].setKind(i);
  }
}

void TorSwitch::handleMessage(cMessage *msg)
{
  // is this a self-message?
  if (msg->isSelfMessage()) {
    // yes! this indicates that a packet has been completed transmitted on the
    // output link

    // get port id
    uint8_t port_id = msg->getKind();

    // are there more packets waiting to be sent from buffer?
    if (!m_queues[port_id].isEmpty()) {
      send_packet_from_buffer_to_node(port_id);
    }
  } else {
    // packet arriving
    handle_packet((Packet *)msg);
  }
}

void TorSwitch::handle_packet(Packet *pkt)
{
  // get name of the gate the packet arrived on
  const char *arrival_gate = pkt->getArrivalGate()->getName();

  if (strcmp(arrival_gate, "generators") == 0) {
    // packet arrived from a generator

    // select an output port
    uint8_t output_port_id = select_output_port(pkt);

    // send packet to node
    send_packet_to_node(pkt, output_port_id);
  } else if (strcmp(arrival_gate, "nodes$i") == 0) {
    // packet arrived from a node

    // determine from which node the packet arrived
    uint8_t arrival_port_id = pkt->getArrivalGate()->getIndex();

    if (pkt->is_processing_done()) {
      // hop count may not exceed number of nodes in ring
      ASSERT(pkt->get_hop_cnt() <= m_n_ports_nodes);

      // processing of packet has been completed. send it to sink with same port
      // id as the node arrival port
      send_packet_to_sink(pkt, arrival_port_id);
    } else {
      // packet may not have completed one ring yet
      ASSERT(pkt->get_hop_cnt() < m_n_ports_nodes);

      // packet has not been processed yet, forward to next node
      uint8_t output_port_id;
      if (pkt->get_flow()->get_toeplitz_hash() % 2 == 0) {
        // forward "right"
        output_port_id = (arrival_port_id + 1) % m_n_ports_nodes;
      } else {
        // forward "left"
        output_port_id =
            (arrival_port_id > 0) ? arrival_port_id - 1 : m_n_ports_nodes - 1;
      }

      // send packet to node
      send_packet_to_node(pkt, output_port_id);
    }
  } else {
    ASSERT(false && "packet arrived on invalid port");
  }
}

void TorSwitch::send_packet_to_node(cPacket *pkt, uint8_t port_id)
{
  ASSERT(port_id < m_n_ports_nodes);

  // place packet in output buffer
  m_queues[port_id].insert(pkt);

  if (!m_self_msgs[port_id].isScheduled()) {
    send_packet_from_buffer_to_node(port_id);
  }
}

void TorSwitch::send_packet_to_sink(cPacket *pkt, uint8_t port_id)
{
  // number of node ports is equal to number of sink ports
  ASSERT(port_id < m_n_ports_nodes);

  send(pkt, "sinks", port_id);
}

void TorSwitch::send_packet_from_buffer_to_node(uint8_t port_id)
{
  // make sure queue is not empty
  ASSERT(!m_queues[port_id].isEmpty());

  // make sure channel is not busy
  ASSERT(!m_channels_nodes[port_id]->isBusy());

  // get packet from queue
  cPacket *pkt = m_queues[port_id].pop();

  // calculate how long the packet has been waiting in the buffer
  simtime_t t_buffer = simTime() - pkt->getArrivalTime();

  // get packet's latency object
  Latency *latency = ((Packet *)pkt)->get_latency();

  // add latency element
  LatencyElement *latency_buffer =
      new LatencyElement(LatencyElement::TOR, t_buffer);
  latency->add_element(latency_buffer);

  // send packet
  send(pkt, "nodes$o", port_id);

  // when will transmission be done?
  simtime_t t_done = m_channels_nodes[port_id]->getTransmissionFinishTime();
  ASSERT(t_done > simTime());

  // schedule self-message
  scheduleAt(t_done, &m_self_msgs[port_id]);
}

uint8_t TorSwitch::select_output_port(Packet *pkt)
{
  // get crc32 hash
  uint32_t hash = pkt->get_flow()->get_crc32_hash();

  // calculate and return output port
  return hash % m_n_ports_nodes;
}