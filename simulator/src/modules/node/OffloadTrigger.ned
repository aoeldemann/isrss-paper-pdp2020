package isrss_sim.modules.node;

simple OffloadTrigger
{
  parameters:
    bool enabled;
    int n_rx_queues;
    int threshold;

  gates:
    output offloadTriggerOut;
}
