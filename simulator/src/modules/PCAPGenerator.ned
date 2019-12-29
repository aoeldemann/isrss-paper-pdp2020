package isrss_sim.modules;

simple PCAPGenerator
{
  parameters:
    string filename_pcap;
    string filename_pcap_ts;
    string filename_ipp;
    string filename_crc32;
    string filename_toeplitz;
    string filename_ids;

  gates:
    output out;
}
