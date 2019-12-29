#include "PCAPGenerator.h"
#include "../defines.h"
#include "../msgs/Packet.h"
#include <netinet/ip.h>

Define_Module(PCAPGenerator);

PCAPGenerator::PCAPGenerator()
{
  m_self_msg = new cMessage();
  m_self_msg->setContextPointer(NULL);
}

PCAPGenerator::~PCAPGenerator()
{
  Packet *packet = (Packet *)m_self_msg->getContextPointer();
  if (packet) {
    delete packet;
  }

  cancelAndDelete(m_self_msg);

  // delete flows
  std::map<uint64_t, Flow *>::iterator it;
  for (it = m_flows.begin(); it != m_flows.end(); it++) {
    delete it->second;
  }
  m_flows.clear();
}

void PCAPGenerator::initialize()
{
  // get filenames
  const char *filename_pcap = par("filename_pcap");
  const char *filename_pcap_ts = par("filename_pcap_ts");
  const char *filename_ipp = par("filename_ipp");
  const char *filename_crc32 = par("filename_crc32");
  const char *filename_toeplitz = par("filename_toeplitz");
  const char *filename_ids = par("filename_ids");

  // open pcap file
  char pcap_errbuf[PCAP_ERRBUF_SIZE];
  m_pcap_descr = pcap_open_offline(filename_pcap, pcap_errbuf);
  if (m_pcap_descr == NULL) {
    throw cRuntimeError("could not open pcap file");
  }

  // open timestamp file
  m_file_pcap_ts.open(filename_pcap_ts);
  if (m_file_pcap_ts.is_open() == false) {
    throw cRuntimeError("could not open pcap timestamp file");
  }

  // open file containing ipp values
  m_file_ipp.open(filename_ipp);
  if (m_file_ipp.is_open() == false) {
    throw cRuntimeError("could not open ipp file");
  }

  // open file containing crc32 hashes
  m_file_crc32.open(filename_crc32);
  if (m_file_crc32.is_open() == false) {
    throw cRuntimeError("could not open crc32 hash file");
  }

  // open file containing toeplitz hashes
  m_file_toeplitz.open(filename_toeplitz);
  if (m_file_toeplitz.is_open() == false) {
    throw cRuntimeError("could not toeplitz hash file");
  }

  // open file containing flow and packet ids (if specified)
  m_file_ids.open(filename_ids);
  if (m_file_ids.is_open() == false) {
    throw cRuntimeError("could not open id file");
  }

  // get output transmission channel
  m_out_channel = gate("out")->getTransmissionChannel();

  // schedule first packet for transmission
  schedule_pcap_packet(true);
}

void PCAPGenerator::handleMessage(cMessage *msg)
{
  if (msg->isSelfMessage() == false) {
    throw cRuntimeError(
        "generator should never receive messages from other modules");
  }

  if (msg->getContextPointer()) {
    // send out the generated packet
    send((Packet *)msg->getContextPointer(), "out");

    // reset context pointer
    msg->setContextPointer(NULL);
  }

  // schedule next packet for transmission
  schedule_pcap_packet(false);
}

void PCAPGenerator::schedule_pcap_packet(bool first)
{
  pcap_pkthdr *pkt_hdr;
  const uint8_t *pkt;

  // get the next packet from pcap file
  int32_t ret = pcap_next_ex(m_pcap_descr, &pkt_hdr, &pkt);
  if (ret == 1) {
    // all good
  } else if (ret == -2) {
    // no more packets
    return;
  } else {
    throw cRuntimeError("could not read from pcap file");
  }

  simtime_t t;

  // get the next timestamp
  std::string file_pcap_ts_line;
  std::getline(m_file_pcap_ts, file_pcap_ts_line);

  // find the decimal point
  for (size_t i = 0; i < file_pcap_ts_line.length(); i++) {
    if (file_pcap_ts_line[i] == '.') {
      // found it!
      if (first) {
        m_pcap_offset_sec = atol(file_pcap_ts_line.substr(0, i).c_str());
      } else {
        t = atol(file_pcap_ts_line.substr(0, i).c_str()) - m_pcap_offset_sec;
      }
      t += atof((std::string("0.") + file_pcap_ts_line.substr(i + 1)).c_str());
      break;
    }
  }

  // get next packet/flow id line from file
  std::string ids_str;
  std::getline(m_file_ids, ids_str);
  const char *ids = ids_str.c_str();

  // separate flow and packet ids (':' seperated)
  char *p = strchr((char *)ids, ':');
  ASSERT(p);
  *p = 0;

  // get flow and packet ids
  uint64_t flow_id = atol(ids);
  uint64_t pkt_id = atol(p + 1);

  // get toeplitz hash value from file
  std::string toeplitz_hash_str;
  std::getline(m_file_toeplitz, toeplitz_hash_str);
  uint32_t toeplitz_hash = atol(toeplitz_hash_str.c_str());

  // get crc32 hash value from file
  std::string crc32_hash_str;
  std::getline(m_file_crc32, crc32_hash_str);
  uint32_t crc32_hash = atol(crc32_hash_str.c_str());

  // get/create flow
  Flow *flow;

  // see if an instance of the flow has already been created
  std::map<uint64_t, Flow *>::const_iterator flow_iter = m_flows.find(flow_id);
  if (flow_iter != m_flows.end()) {
    // flow found!
    flow = flow_iter->second;

    // flow has already been created, so this is not the first packet of it
    ASSERT(pkt_id > 0);

    // make sure flow fields match
    ASSERT(flow->get_id() == flow_id);
    ASSERT(flow->get_toeplitz_hash() == toeplitz_hash);
    ASSERT(flow->get_crc32_hash() == crc32_hash);
  } else {
    // flow not found! create a new one
    flow = new Flow(flow_id);

    // must be the first packet of this flow
    ASSERT(pkt_id == 0);

    // set flow fields
    flow->set_toeplitz_hash(toeplitz_hash);
    flow->set_crc32_hash(crc32_hash);

    // save flow, reusing it later
    m_flows.insert(std::pair<uint64_t, Flow *>(flow_id, flow));
  }

  // create new packet and set the flow
  Packet *packet = new Packet();
  packet->set_flow(flow);

  // generation time is when packet has been completely transmitted on the
  // link
  // TODO: currently multiplied by 2. move to sink
  simtime_t t_generation = t + 2.0 * (8.0 * ((double)pkt_hdr->len) /
                                      m_out_channel->getNominalDatarate());

  packet->setByteLength(pkt_hdr->len);
  packet->get_latency()->set_t_generation(t_generation);
  packet->setKind(MSG_KIND_PACKET_DATA);

  // get ipp value from file
  std::string instr_str;
  std::getline(m_file_ipp, instr_str);
  uint32_t instr = atol(instr_str.c_str());

  // set number of instructions to be executed on this packet
  packet->set_instr(instr);

  // save packet id in packet
  packet->set_id(pkt_id);

  // set sel message's context pointer to point to the generated packet
  m_self_msg->setContextPointer(packet);

  // schedule packet transmission
  scheduleAt(t, m_self_msg);
}
