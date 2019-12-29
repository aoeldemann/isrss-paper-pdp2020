#include "Latency.h"

LatencyElement::LatencyElement(latency_type_t type, simtime_t latency)
    : m_type(type), m_latency(latency)
{
}

LatencyElement::latency_type_t LatencyElement::get_type() { return m_type; }

simtime_t LatencyElement::get_latency() { return m_latency; }

Latency::Latency() {}

Latency::~Latency()
{
  for (iterator it = begin(); it != end(); it++) {
    delete *it;
  }
  clear();
}

simtime_t Latency::get_end_to_end_latency()
{
  return simTime() - m_t_generation;
}

void Latency::add_element(LatencyElement *element) { push_back(element); }

simtime_t Latency::get_total_latency()
{
  simtime_t latency;
  for (iterator it = begin(); it != end(); it++) {
    latency += (*it)->get_latency();
  }
  return latency;
}

simtime_t
Latency::get_total_latency_by_type(LatencyElement::latency_type_t type)
{
  simtime_t latency;
  for (iterator it = begin(); it != end(); it++) {
    if ((*it)->get_type() == type) {
      latency += (*it)->get_latency();
    }
  }
  return latency;
}

void Latency::set_t_generation(simtime_t t_generation)
{
  m_t_generation = t_generation;
}
