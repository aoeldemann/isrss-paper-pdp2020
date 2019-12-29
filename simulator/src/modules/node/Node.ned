package isrss_sim.modules.node;

module Node
{
    parameters:
        int n_ports;
        int n_cores;
        int max_hop_cnt;
        bool enable_balance_cores;
        bool enable_offload;

        string type_processing;

    gates:
        inout ports[n_ports];

    submodules:
        ingress: Ingress;
        offload: Offload {
            enable_balance_cores = enable_balance_cores;
            enable_offload = enable_offload;
            n_rx_queues = n_cores;
            max_hop_cnt = max_hop_cnt;
        };
        offload_trigger: OffloadTrigger {
            enabled = enable_balance_cores || enable_offload;
            n_rx_queues = n_cores;
        };
        proc: <type_processing> like IProcessing {
          n_cores = n_cores;
        };
        out_buffer[n_ports]: OutputBuffer;

    connections:
        for i=0..n_ports-1 {
            ports$i[i] --> ingress.in++;
            ingress.out++ --> offload.in++;
            offload.out++ --> out_buffer[i].in++;
            out_buffer[i].out --> ports$o[i];
        }

        offload.out_proc --> proc.in;
        proc.out --> out_buffer[0].in++;

        offload_trigger.offloadTriggerOut --> offload.offloadTriggerIn;
}
