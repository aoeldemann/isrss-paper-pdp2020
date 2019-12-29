#ifndef MODULES_NODE_INGRESS_H_
#define MODULES_NODE_INGRESS_H_

#include <omnetpp.h>

using namespace omnetpp;

class Ingress : public cSimpleModule
{
protected:
  virtual void handleMessage(cMessage *msg);
};

#endif
