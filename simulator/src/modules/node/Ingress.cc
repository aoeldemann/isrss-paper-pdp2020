#include "Ingress.h"
#include "../../defines.h"
#include "../../msgs/Packet.h"
#include "../../msgs/PacketNodeContext.h"

Define_Module(Ingress);

void Ingress::handleMessage(cMessage *msg)
{
  // get arrival port id
  int arrival_port_id = msg->getArrivalGate()->getIndex();

  if (msg->getKind() == MSG_KIND_PACKET_DATA) {
    // cast packet
    Packet *pkt = (Packet *)msg;

    // get node context
    PacketNodeContext *ctx = pkt->get_node_ctx();

    // save arrival gate id
    ctx->set_arrival_port_id((uint8_t)arrival_port_id);
  }

  // send out packet
  send(msg, "out", arrival_port_id);
}
