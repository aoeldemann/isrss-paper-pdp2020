package isrss_sim.modules.node;

moduleinterface IProcessing {
  parameters:
    int n_cores;
  gates:
    input in;
    output out;
}
