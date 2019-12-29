package isrss_sim.modules.node;

simple ProcessingDynamicRSS extends Processing
{
  parameters:
    @class(ProcessingDynamicRSS);

    int rss_reta_size;
    double t_reassignment_interval;
}
