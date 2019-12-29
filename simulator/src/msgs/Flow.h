#ifndef MSGS_FLOW_H_
#define MSGS_FLOW_H_

#include <omnetpp.h>

struct Flow {

  Flow(uint64_t id)
  {
    m_id = id;
    m_crc32_hash = 0;
    m_crc32_hash_set = false;
    m_toeplitz_hash = 0;
    m_toeplitz_hash_set = false;
  }

  virtual ~Flow() {}

  uint64_t get_id() { return m_id; }

  uint32_t get_crc32_hash()
  {
    ASSERT(m_crc32_hash_set);
    return m_crc32_hash;
  }

  void set_crc32_hash(uint32_t hash)
  {
    ASSERT(!m_crc32_hash_set);
    m_crc32_hash = hash;
    m_crc32_hash_set = true;
  }

  uint32_t get_toeplitz_hash()
  {
    ASSERT(m_toeplitz_hash_set);
    return m_toeplitz_hash;
  }

  void set_toeplitz_hash(uint32_t hash)
  {
    ASSERT(!m_toeplitz_hash_set);
    m_toeplitz_hash = hash;
    m_toeplitz_hash_set = true;
  }

private:
  uint64_t m_id;
  uint32_t m_crc32_hash;
  bool m_crc32_hash_set;
  uint32_t m_toeplitz_hash;
  bool m_toeplitz_hash_set;
};

#endif
