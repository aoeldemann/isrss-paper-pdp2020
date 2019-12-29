#include "ProcessingDynamicRSS.h"
#include "../../msgs/Packet.h"
#include "Offload.h"

Define_Module(ProcessingDynamicRSS)

    ProcessingDynamicRSS::~ProcessingDynamicRSS()
{
  delete[] m_core_instr_cntr;
  delete[] m_rss_reta;
  delete[] m_rss_reta_instr_cntr;
}

void ProcessingDynamicRSS::initialize()
{
  // initialize parent module
  Processing::initialize();

  // initialize per-core instruction counter
  m_core_instr_cntr = new uint64_t[m_n_cores];
  for (uint8_t i = 0; i < m_n_cores; i++) {
    m_core_instr_cntr[i] = 0;
  }

  // get parameters
  m_t_reassignment_interval = par("t_reassignment_interval");
  m_rss_reta_size = par("rss_reta_size");

  // initialize rss reta table and per-entry instruction counters
  m_rss_reta = new uint8_t[m_rss_reta_size];
  m_rss_reta_instr_cntr = new uint64_t[m_rss_reta_size];
  for (uint16_t i = 0; i < m_rss_reta_size; i++) {
    m_rss_reta[i] = i % m_n_cores;
    m_rss_reta_instr_cntr[i] = 0;
  }

  // get pointer on offload module
  m_module_offload = (Offload *)getModuleByPath("^.offload");
  ASSERT(m_module_offload);
}

Packet *ProcessingDynamicRSS::process_packet(uint8_t core_id)
{
  // process packet
  Packet *pkt = Processing::process_packet(core_id);

  // how many instructions have been processed on this packet?
  uint32_t n_instr = pkt->get_instr();

  // get packets toeplitz hash value
  uint32_t toeplitz_hash = pkt->get_flow()->get_toeplitz_hash();

  // update per-core instruction counter
  m_core_instr_cntr[core_id] += n_instr;

  // update per-reta entry instruction counter
  m_rss_reta_instr_cntr[toeplitz_hash % m_rss_reta_size] += n_instr;

  // perform RSS reassignment?
  if (simTime() - m_t_last_reassignment >= m_t_reassignment_interval) {
    // find core with the highest and lowest loads
    uint8_t core_id_highest_load, core_id_lowest_load;
    uint64_t n_instr_max, n_instr_min;
    for (uint8_t i = 0; i < m_n_cores; i++) {
      if ((i == 0) || (m_core_instr_cntr[i] > n_instr_max)) {
        core_id_highest_load = i;
        n_instr_max = m_core_instr_cntr[i];
      }
      if ((i == 0) || (m_core_instr_cntr[i] < n_instr_min)) {
        core_id_lowest_load = i;
        n_instr_min = m_core_instr_cntr[i];
      }
    }

    // find redirection table entry that causes the highest load on the core
    // that has the highest load
    uint16_t reta_entry_highest_load;
    bool reta_entry_found = false;
    for (uint16_t i = 0; i < m_rss_reta_size; i++) {
      if (m_rss_reta[i] != core_id_highest_load) {
        continue;
      }

      if (!reta_entry_found || (m_rss_reta_instr_cntr[i] > n_instr_max)) {
        reta_entry_highest_load = i;
        reta_entry_found = true;
        n_instr_max = m_rss_reta_instr_cntr[i];
      }
    }

    // update reta entry
    if (reta_entry_found) {
      m_rss_reta[reta_entry_highest_load] = core_id_lowest_load;
      m_module_offload->update_rss_reta_entry(reta_entry_highest_load,
                                              core_id_lowest_load);
    }

    // reset all per-core instruction counters
    for (uint8_t i = 0; i < m_n_cores; i++) {
      m_core_instr_cntr[i] = 0;
    }

    // reset all reta instruction counters
    for (uint16_t i = 0; i < m_rss_reta_size; i++) {
      m_rss_reta_instr_cntr[i] = 0;
    }

    // save reassignment time
    m_t_last_reassignment = simTime();
  }

  // return packet
  return pkt;
}
