#include <assert.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pcap.h>
#include <stdlib.h>

#define PCAP_ERRBUF_SIZE 256

// intel i40e hash key
uint32_t const rss_key[] = {0x4439796b, 0xb54c5023, 0xb675ea5b, 0x124f9f30,
                            0xb8a2c03d, 0xdfdc4d02, 0xa08c9b33, 0x4af64a4c,
                            0x05c6fa34, 0x3958d855, 0x7d99583a, 0xe138c92e,
                            0x81150366};

uint32_t toeplitz_hash_ipv4(uint32_t daddr, uint32_t saddr, uint8_t trace_id)
{
  uint8_t data[12];
  *((uint32_t *)(data)) = saddr;
  *((uint32_t *)(data + 4)) = daddr;
  *((uint32_t *)(data + 8)) = trace_id;
  uint32_t ret = 0;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 32; i++) {
      if (((uint32_t *)data)[j] & (1 << (31 - i))) {
        ret ^= rss_key[j] << i |
               (uint32_t)((uint64_t)(rss_key[j + 1]) >> (32 - i));
      }
    }
  }
  return ret;
}

uint32_t toeplitz_hash_ipv4_tcp_udp(uint32_t daddr, uint32_t saddr,
                                    uint16_t dport, uint16_t sport,
                                    uint8_t trace_id)
{
  uint8_t data[16];
  *((uint32_t *)(data)) = saddr;
  *((uint32_t *)(data + 4)) = daddr;
  *((uint16_t *)(data + 8)) = dport;
  *((uint16_t *)(data + 10)) = sport;
  *((uint32_t *)(data + 12)) = trace_id;

  uint32_t ret = 0;

  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 32; i++) {
      if (((uint32_t *)data)[j] & (1 << (31 - i))) {
        ret ^= rss_key[j] << i |
               (uint32_t)((uint64_t)(rss_key[j + 1]) >> (32 - i));
      }
    }
  }
  return ret;
}

uint32_t toeplitz_hash_ipv6(const struct in6_addr *daddr,
                            const struct in6_addr *saddr, uint8_t trace_id)
{
  uint8_t data[36];
  *((uint32_t *)(data + 0)) = ntohl(*(((uint32_t *)saddr) + 0));
  *((uint32_t *)(data + 4)) = ntohl(*(((uint32_t *)saddr) + 1));
  *((uint32_t *)(data + 8)) = ntohl(*(((uint32_t *)saddr) + 2));
  *((uint32_t *)(data + 12)) = ntohl(*(((uint32_t *)saddr) + 3));
  *((uint32_t *)(data + 16)) = ntohl(*(((uint32_t *)daddr) + 0));
  *((uint32_t *)(data + 20)) = ntohl(*(((uint32_t *)daddr) + 1));
  *((uint32_t *)(data + 24)) = ntohl(*(((uint32_t *)daddr) + 2));
  *((uint32_t *)(data + 28)) = ntohl(*(((uint32_t *)daddr) + 3));
  *((uint32_t *)(data + 32)) = trace_id;

  uint32_t ret = 0;

  for (int j = 0; j < 9; j++) {
    for (int i = 0; i < 32; i++) {
      if (((uint32_t *)data)[j] & (1 << (31 - i))) {
        ret ^= rss_key[j] << i |
               (uint32_t)((uint64_t)(rss_key[j + 1]) >> (32 - i));
      }
    }
  }
  return ret;
}

uint32_t toeplitz_hash_ipv6_tcp_udp(const struct in6_addr *daddr,
                                    const struct in6_addr *saddr,
                                    uint16_t dport, uint16_t sport,
                                    uint8_t trace_id)
{

  uint8_t data[40];
  *((uint32_t *)(data + 0)) = ntohl(*(((uint32_t *)saddr) + 0));
  *((uint32_t *)(data + 4)) = ntohl(*(((uint32_t *)saddr) + 1));
  *((uint32_t *)(data + 8)) = ntohl(*(((uint32_t *)saddr) + 2));
  *((uint32_t *)(data + 12)) = ntohl(*(((uint32_t *)saddr) + 3));
  *((uint32_t *)(data + 16)) = ntohl(*(((uint32_t *)daddr) + 0));
  *((uint32_t *)(data + 20)) = ntohl(*(((uint32_t *)daddr) + 1));
  *((uint32_t *)(data + 24)) = ntohl(*(((uint32_t *)daddr) + 2));
  *((uint32_t *)(data + 28)) = ntohl(*(((uint32_t *)daddr) + 3));
  *((uint16_t *)(data + 32)) = dport;
  *((uint16_t *)(data + 34)) = sport;
  *((uint32_t *)(data + 36)) = trace_id;

  uint32_t ret = 0;

  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 32; i++) {
      if (((uint32_t *)data)[j] & (1 << (31 - i))) {
        ret ^= rss_key[j] << i |
               (uint32_t)((uint64_t)(rss_key[j + 1]) >> (32 - i));
      }
    }
  }
  return ret;
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    printf("Usage: %s <trace_file> <trace_id>\n", argv[0]);
    return -1;
  }

  char *fname_pcap = argv[1];
  uint8_t trace_id = atoi(argv[2]);

  // open pcap file
  char pcap_errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *pcap_descr = pcap_open_offline(fname_pcap, pcap_errbuf);
  if (!pcap_descr) {
    printf("ERROR: could not open pcap file\n");
    return -1;
  }

  struct pcap_pkthdr *pkt_hdr;
  const uint8_t *pkt;
  uint8_t ip_version, ip_proto;
  uint32_t ipv4_daddr, ipv4_saddr;
  struct in6_addr *ipv6_daddr, *ipv6_saddr;
  uint16_t l4_dport, l4_sport;
  uint32_t hash_val;

  while (pcap_next_ex(pcap_descr, &pkt_hdr, &pkt) == 1) { // rc 1 => done
    ip_version = ((struct iphdr *)pkt)->version;

    // only ipv4 and ipv6, packets must be sufficiently long
    assert(((ip_version == 4) && (pkt_hdr->caplen >= 20)) ||
           ((ip_version == 6) && (pkt_hdr->caplen >= 40)));

    if (ip_version == 4) {
      struct iphdr *hdr = (struct iphdr *)pkt;

      // no fragmented packets
      assert((ntohs(hdr->frag_off) & IP_OFFMASK) == 0);
      assert((ntohs(hdr->frag_off) & IP_MF) == 0);

      ipv4_daddr = ntohl(hdr->daddr);
      ipv4_saddr = ntohl(hdr->saddr);
      ip_proto = hdr->protocol;

      // no ipv6 in ipv4
      assert(ip_proto != 41);
    } else if (ip_version == 6) {
      struct ip6_hdr *hdr = (struct ip6_hdr *)pkt;

      ipv6_daddr = &hdr->ip6_dst;
      ipv6_saddr = &hdr->ip6_src;
      ip_proto = hdr->ip6_ctlun.ip6_un1.ip6_un1_nxt;
    } else {
      assert(0);
    }

    if (ip_proto == 6) {
      struct tcphdr *hdr;
      if (ip_version == 4) {
        hdr = (struct tcphdr *)(pkt + sizeof(struct iphdr));
      } else if (ip_version == 6) {
        hdr = (struct tcphdr *)(pkt + sizeof(struct ip6_hdr));
      }

      l4_dport = ntohs(hdr->dest);
      l4_sport = ntohs(hdr->source);
    } else if (ip_proto == 17) {
      struct udphdr *hdr;
      if (ip_version == 4) {
        hdr = (struct udphdr *)(pkt + sizeof(struct iphdr));
      } else if (ip_version == 6) {
        hdr = (struct udphdr *)(pkt + sizeof(struct ip6_hdr));
      }

      l4_dport = ntohs(hdr->dest);
      l4_sport = ntohs(hdr->source);
    }

    if (ip_version == 4) {
      if ((ip_proto == 6) || (ip_proto == 17)) {
        hash_val = toeplitz_hash_ipv4_tcp_udp(ipv4_daddr, ipv4_saddr, l4_dport,
                                              l4_sport, trace_id);
      } else {
        hash_val = toeplitz_hash_ipv4(ipv4_daddr, ipv4_saddr, trace_id);
      }
    } else if (ip_version == 6) {
      if ((ip_proto == 6) || (ip_proto == 17)) {
        hash_val = toeplitz_hash_ipv6_tcp_udp(ipv6_daddr, ipv6_saddr, l4_dport,
                                              l4_sport, trace_id);
      } else {
        hash_val = toeplitz_hash_ipv6(ipv6_daddr, ipv6_saddr, trace_id);
      }
    } else {
      assert(0);
    }

    printf("%u\n", hash_val);
  }

  return 0;
}
