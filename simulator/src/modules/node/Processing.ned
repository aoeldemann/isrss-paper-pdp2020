package isrss_sim.modules.node;

simple Processing like IProcessing
{
  parameters:
    int n_cores;
    double capacity_per_core;

    @signal[stats_ipp](type="unsigned long");
    @statistic[ipp](source="stats_ipp"; record=stats);

    @signal[stats_proc_util](type="unsigned long");
    @statistic[proc_util](source="stats_proc_util"; record=timeavg);

    @signal[stats_proc_util_core*](type="bool");
    @statisticTemplate[proc_util_core](record=timeavg);

    @signal[stats_queue_len*](type="long");
    @statisticTemplate[queue_len](record=stats);

  gates:
    input in;
    output out;
}
