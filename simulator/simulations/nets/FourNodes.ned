package isrss_sim.simulations.nets;

import isrss_sim.modules.PCAPGenerator;
import isrss_sim.modules.TorSwitch;
import isrss_sim.modules.node.Node;
import isrss_sim.modules.Sink;

network FourNodes {
  parameters:
    int n_generators;

  submodules:
    generators[n_generators]: PCAPGenerator;
    tor: TorSwitch;
    nodes[4]: Node {
      n_ports = 1;
      max_hop_cnt = 4;
    }
    sink: Sink;

  connections:
    for i=0..n_generators-1 {
      generators[i].out --> { datarate = 10Gbps; } --> tor.generators++;
    }

    for i=0..3 {
      tor.nodes++ <--> { datarate = (n_generators/4)*10Gbps; } <--> nodes[i].ports++;
      tor.sinks++ --> sink.in++;
    }
}
