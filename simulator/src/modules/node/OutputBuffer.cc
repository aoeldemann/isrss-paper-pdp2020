#include "OutputBuffer.h"
#include "../../defines.h"
#include "../../msgs/Packet.h"
#include "../../msgs/PacketNodeContext.h"
#include "Processing.h"

Define_Module(OutputBuffer);

OutputBuffer::~OutputBuffer() { cancelAndDelete(m_self_msg); }

void OutputBuffer::initialize()
{
  m_waiting_for_input = true;
  m_self_msg = new cMessage();

  m_out_channel = gate("out")->getTransmissionChannel();
}

void OutputBuffer::handleMessage(cMessage *msg)
{
  if (msg->isSelfMessage()) {
    // packet transmission on the link is done
    if (m_pkt_queue.getLength() == 0) {
      // no more packets to be sent. do nothing.
      m_waiting_for_input = true;
    } else {
      // send next packet
      send_packet();
    }
  } else {
    // packet arriving. insert into queue
    cPacket *pkt = (cPacket *)msg;
    m_pkt_queue.insert(pkt);

    // if no transmission on the output link is ongoing, send the packet right
    // away
    if (m_waiting_for_input) {
      m_waiting_for_input = false;
      send_packet();
    }
  }
}

void OutputBuffer::send_packet()
{
  // pop packet form queue
  cPacket *msg = (cPacket *)m_pkt_queue.pop();

  if (msg->getKind() == MSG_KIND_PACKET_DATA) {
    // this is a data packet
    Packet *pkt = (Packet *)msg;

    // increment its hop count
    pkt->incrm_hop_cnt();

    // clear node context
    pkt->get_node_ctx()->clear();

    // get the time the packet spent in the output buffer
    simtime_t t_lat_buffer = simTime() - pkt->getArrivalTime();

    // ... and record this time
    Latency *latency = pkt->get_latency();
    LatencyElement *latency_buffer =
        new LatencyElement(LatencyElement::NODE_BUFFER_OUT, t_lat_buffer);
    latency->add_element(latency_buffer);
  }

  // send packet out
  send(msg, "out");

  // schedule self message that is triggered once the packet has completely been
  // transmitted
  scheduleAt(m_out_channel->getTransmissionFinishTime(), m_self_msg);
}
