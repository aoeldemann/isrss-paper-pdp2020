package isrss_sim.modules;

simple Sink
{
  parameters:
    bool check_reorder = default(false);

    @signal[stats_n_packets](type="long");
    @statistic[n_packets](source="stats_n_packets"; record=count);

    @signal[stats_lat_end_to_end](type="simtime_t");
    @statistic[lat_end_to_end](source="stats_lat_end_to_end"; record=stats);

    @signal[stats_lat_node_buffer_in](type="simtime_t");
    @statistic[lat_node_buffer_in](source="stats_lat_node_buffer_in"; record=stats);

    @signal[stats_lat_node_buffer_out](type="simtime_t");
    @statistic[lat_node_buffer_out](source="stats_lat_node_buffer_out"; record=stats);

    @signal[stats_lat_node_proc](type="simtime_t");
    @statistic[lat_node_proc](source="stats_lat_node_proc"; record=stats);

    @signal[stats_lat_tor](type="simtime_t");
    @statistic[lat_tor](source="stats_lat_tor"; record=stats);

    @signal[stats_hop_cnt](type="unsigned long");
    @statistic[hop_cnt](source="stats_hop_cnt"; record=histogram);

    @signal[stats_reorder_check_n_reordered_pkts](type="long");
    @statistic[reorder_check_n_reordered_pkts](source="stats_reorder_check_n_reordered_pkts"; record=count);

    @signal[stats_reorder_check_n_reordered_flows](type="long");
    @statistic[reorder_check_n_reordered_flows](source="stats_reorder_check_n_reordered_flows"; record=count);

  gates:
    input in[];
}
