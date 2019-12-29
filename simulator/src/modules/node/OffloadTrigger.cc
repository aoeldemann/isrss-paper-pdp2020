#include "OffloadTrigger.h"
#include "../../defines.h"
#include "../../msgs/OffloadTriggerMsg_m.h"
#include "../../msgs/Packet.h"

Define_Module(OffloadTrigger)

    OffloadTrigger::~OffloadTrigger()
{
  if (m_enabled) {
    delete[] m_offload_enabled;
  }
}

void OffloadTrigger::initialize()
{
  // offloading enabled?
  m_enabled = par("enabled");

  if (m_enabled == false) {
    // nothing more to do
    return;
  }

  // get offloading threshold
  m_threshold = par("threshold");

  // get number of rx queues
  m_n_rx_queues = par("n_rx_queues");

  // initially disable offloading on all queues
  m_offload_enabled = new bool[m_n_rx_queues];
  for (uint8_t i = 0; i < m_n_rx_queues; i++) {
    m_offload_enabled[i] = false;
  }
}

void OffloadTrigger::handleMessage(cMessage *msg)
{
  throw cRuntimeError("this module should not receive messages");
}

bool OffloadTrigger::is_offload_enabled(uint8_t queue_id)
{
  // do dome error checking
  ASSERT(m_enabled);
  ASSERT(queue_id < m_n_rx_queues);

  // return whether the queue is marked for traffic offloading
  return m_offload_enabled[queue_id];
}

void OffloadTrigger::set_offload_enable(uint8_t queue_id, bool enable)
{
  Enter_Method_Silent();

  // do some error checking
  ASSERT(m_enabled);
  ASSERT(queue_id < m_n_rx_queues);
  ASSERT(m_offload_enabled[queue_id] != enable);

  // mark/unmark queue for traffic offloading locally for the operation of this
  // module
  m_offload_enabled[queue_id] = enable;

  // then also send a trigger message to the offload module informing it of the
  // updated queue state
  OffloadTriggerMsg *msg = new OffloadTriggerMsg;
  msg->setKind(MSG_KIND_OFFLOAD_TRIGGER);
  msg->setQueueId(queue_id);
  msg->setActive(enable);
  send(msg, "offloadTriggerOut");
}

void OffloadTrigger::report_queue_len(uint8_t queue_id, uint32_t queue_len)
{
  if (m_enabled) {
    if (is_offload_enabled(queue_id)) {
      if (queue_len <= m_threshold) {
        // traffic for this queue is currently being offloaded. now it's below
        // the threshold again, so disable offloading
        set_offload_enable(queue_id, false);
      }
    } else {
      if (queue_len > m_threshold) {
        // traffic for this queue is currently not being offloaded. no it's
        // above the threshold. so enable offloading
        set_offload_enable(queue_id, true);
      }
    }
  }
}
