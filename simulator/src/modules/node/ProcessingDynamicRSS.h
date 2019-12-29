#ifndef MODULES_NODE_PROCESSINGDYNAMICRSS_H_
#define MODULES_NODE_PROCESSINGDYNAMICRSS_H_

#include "Processing.h"

class Offload;

class ProcessingDynamicRSS : public Processing
{
public:
  virtual ~ProcessingDynamicRSS();
  virtual void initialize();

private:
  virtual Packet *process_packet(uint8_t core_id);

  uint16_t m_rss_reta_size;
  uint8_t *m_rss_reta;

  uint64_t *m_core_instr_cntr;
  uint64_t *m_rss_reta_instr_cntr;

  simtime_t m_t_last_reassignment;
  simtime_t m_t_reassignment_interval;

  Offload *m_module_offload;
};

#endif
