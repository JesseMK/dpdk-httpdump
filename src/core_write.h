#ifndef HTTPDUMP_CORE_WRITE_H
#define HTTPDUMP_CORE_WRITE_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_ether.h>

#define MAX_LCORES 32
#define DPDKCAP_WRITE_BURST_SIZE 512
#define RTE_ETH_PCAP_SNAPLEN ETHER_MAX_JUMBO_FRAME_LEN

static unsigned char tx_pcap_data[RTE_ETH_PCAP_SNAPLEN];

static struct rte_ring *write_ring[MAX_LCORES];

/* Statistics structure */
struct core_write_stats
{
  int core_id;
  uint64_t current_file_packets;
  uint64_t packets;
  uint64_t packets_dropped;
};

/* Writing core configuration */
struct core_write_config
{
  struct rte_ring *ring;
  volatile uint8_t * stop_condition;
  struct core_write_stats *stats;
  unsigned int snaplen;
  unsigned long rotate_seconds;
};

static struct core_write_stats *cores_stats_write_list;

static void eth_pcap_gather_data(unsigned char *data, struct rte_mbuf *mbuf);

/* Launches a analysis task */
int write_core(const struct core_write_config *config);

static struct timeval start_time;
static uint64_t start_cycles;
static uint64_t hz;

static void init_time(void);
static inline void calculate_timestamp(struct timeval *ts);

#endif
