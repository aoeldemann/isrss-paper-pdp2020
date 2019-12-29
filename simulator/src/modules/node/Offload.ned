package isrss_sim.modules.node;

simple Offload
{
  parameters:
    bool enable_balance_cores;
    bool enable_offload;
    int n_rx_queues;
    double hashtable_entry_timeout;
    int hashtable_size;
    int max_hop_cnt;

    gates:
      input in[];
      output out[];
      output out_proc;
      input offloadTriggerIn;
}
