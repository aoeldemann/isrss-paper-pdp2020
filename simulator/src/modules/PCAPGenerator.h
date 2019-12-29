#ifndef MODULES_PCAPGENERATOR_H_
#define MODULES_PCAPGENERATOR_H_

#include "../msgs/Flow.h"
#include <fstream>
#include <omnetpp.h>
#include <pcap.h>

using namespace omnetpp;

class PCAPGenerator : public cSimpleModule
{
public:
  PCAPGenerator();
  virtual ~PCAPGenerator();

protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);

  void schedule_pcap_packet(bool first);

private:
  cMessage *m_self_msg;
  cChannel *m_out_channel;

  pcap_t *m_pcap_descr;
  __time_t m_pcap_offset_sec;

  std::ifstream m_file_pcap_ts;
  std::ifstream m_file_ipp;
  std::ifstream m_file_crc32;
  std::ifstream m_file_toeplitz;
  std::ifstream m_file_ids;

  std::map<uint64_t, Flow *> m_flows;
};

#endif
