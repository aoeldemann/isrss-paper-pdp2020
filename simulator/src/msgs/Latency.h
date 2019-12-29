#ifndef MSGS_LATENCY_H_
#define MSGS_LATENCY_H_

#include <omnetpp.h>

using namespace omnetpp;

class LatencyElement
{
public:
  typedef enum {
    NODE_BUFFER_IN,
    NODE_BUFFER_OUT,
    NODE_PROC,
    TOR
  } latency_type_t;

  LatencyElement(latency_type_t type, simtime_t latency);

  latency_type_t get_type();
  simtime_t get_latency();

private:
  latency_type_t m_type;
  simtime_t m_latency;
};

class Latency : public std::vector<LatencyElement *>
{
public:
  Latency();
  virtual ~Latency();

  void add_element(LatencyElement *element);
  simtime_t get_end_to_end_latency();
  simtime_t get_total_latency();
  simtime_t get_total_latency_by_type(LatencyElement::latency_type_t type);

  void set_t_generation(simtime_t t_generation);

private:
  simtime_t m_t_generation;
};

#endif
