#ifndef MODULES_NODE_OUTPUTBUFFER_H_
#define MODULES_NODE_OUTPUTBUFFER_H_

#include <omnetpp.h>

using namespace omnetpp;

class OutputBuffer : public cSimpleModule
{
public:
  virtual ~OutputBuffer();

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);

private:
  void send_packet();

  cPacketQueue m_pkt_queue;

  bool m_waiting_for_input;
  cMessage *m_self_msg;
  cChannel *m_out_channel;
};

#endif
