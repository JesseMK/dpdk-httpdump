#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pcap.h>

#include <rte_ring.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_branch_prediction.h>

#include "core_write.h"
#include "hd_pcap.h"
#include "hd_pkt.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define RTE_LOGTYPE_HTTPDUMP RTE_LOGTYPE_USER1

static void
init_time(void)
{

#ifdef DEBUG
    printf("init time...\n");
#endif

    gettimeofday(&start_time, NULL);
    start_cycles = rte_get_timer_cycles();
    hz = rte_get_timer_hz();

#ifdef DEBUG
    printf("start_cycles: %lu\n", start_cycles);
    printf("hz: %lu\n", hz);
#endif
}

static inline void
calculate_timestamp(struct timeval *ts)
{
    uint64_t cycles;
    struct timeval cur_time;

    cycles = rte_get_timer_cycles() - start_cycles;
    cur_time.tv_sec = cycles / hz;
    cur_time.tv_usec = (cycles % hz) * 1e6 / hz;
    timeradd(&start_time, &cur_time, ts);
}

/* Copy data from mbuf chain to a buffer suitable for writing to a PCAP file. */
static void
eth_pcap_gather_data(unsigned char *data, struct rte_mbuf *mbuf)
{
    uint16_t data_len = 0;

    while (mbuf)
    {
        rte_memcpy(data + data_len, rte_pktmbuf_mtod(mbuf, void *),
                   mbuf->data_len);

        data_len += mbuf->data_len;
        mbuf = mbuf->next;
    }
}

void rehash(void)
{
    int lcore_id = rte_lcore_id();
    if (hashtable_old[lcore_id])
    {
        record_t *record, *tmp;
        HASH_ITER(hh, hashtable_old[lcore_id], record, tmp)
        {
            httpdump_http(record->data, REASM_MAX_LENGTH, record->ts, &(record->key.src), &(record->key.dst));
            HASH_DEL(hashtable_old[lcore_id], record);
            free(record);
        }
    }
    hashtable_old[lcore_id] = NULL;
    if (hashtable[lcore_id])
    {
        record_t *record, *tmp;
        HASH_ITER(hh, hashtable[lcore_id], record, tmp)
        {
            httpdump_http(record->data, REASM_MAX_LENGTH, record->ts, &(record->key.src), &(record->key.dst));
            HASH_DEL(hashtable[lcore_id], record);
            free(record);
        }
    }
    hashtable[lcore_id] = NULL;
}

/*
 * Write the packets form the write ring into a pcap compressed file
 */
int write_core(const struct core_write_config *config)
{
    int to_write;
    int retval = 0;
    struct rte_mbuf *dequeued[HTTPDUMP_WRITE_BURST_SIZE];
    struct rte_mbuf *mbuf;
    struct pcap_pkthdr header;

    //Init stats
    *(config->stats) = (struct core_write_stats){
        .core_id = rte_lcore_id(),
        .current_file_packets = 0,
        .packets_dropped = 0,
        .packets = 0,
    };

    init_time();
    init_hashtable(config->stats->core_id);

    for (;;)
    {
        if (unlikely((*(config->stop_condition)) && rte_ring_empty(config->ring)))
        {
#ifdef DEBUG
            printf("CORE %u: Exit cause stop_condition is :%u\n", config->stats->core_id, *(config->stop_condition));
#endif
            break;
        }

#ifdef DEBUG
        printf("CORE %u: %d packets to write\n", config->stats->core_id, to_write);
#endif

        //Get packets from the ring
        to_write = rte_ring_dequeue_bulk(config->ring, (void *)dequeued,
                                         HTTPDUMP_WRITE_BURST_SIZE, NULL);
        if (unlikely(to_write == 0))
            to_write = rte_ring_dequeue_burst(config->ring, (void *)dequeued,
                                              HTTPDUMP_WRITE_BURST_SIZE, NULL);

        //Update stats
        config->stats->packets += to_write;

        int i;
        for (i = 0; i < to_write; i++)
        {

#ifdef DEBUG
            printf("CORE %u: package %d\n", config->stats->core_id, i);
#endif
            //Cast to packet
            mbuf = dequeued[i];

#ifdef DEBUG
            printf("CORE %u: mbuf->pkt_len: %u\n", config->stats->core_id, mbuf->pkt_len);
#endif

            calculate_timestamp(&header.ts);

            header.len = mbuf->pkt_len;
            header.caplen = header.len;

            if (likely(mbuf->nb_segs == 1))
            {
                httpdump_pcap(&header, rte_pktmbuf_mtod(mbuf, void *));
                config->stats->current_file_packets++;
            }
            else
            {
                if (mbuf->pkt_len <= ETHER_MAX_JUMBO_FRAME_LEN)
                {
                    eth_pcap_gather_data(tx_pcap_data, mbuf);
                    httpdump_pcap(&header, tx_pcap_data);
                    config->stats->current_file_packets++;
                }
                else
                {
                    printf("Dropping PCAP packet. Size (%d) > max jumbo size (%d).\n",
                           mbuf->pkt_len,
                           ETHER_MAX_JUMBO_FRAME_LEN);
                    config->stats->packets_dropped++;

                    rte_pktmbuf_free(mbuf);
                    break;
                }
            }

            rte_pktmbuf_free(dequeued[i]);
        }
    }

cleanup:
    rehash();

    printf("------------------------\n");
    printf("Closed writing core %d\n", rte_lcore_id());
    printf("Dequeued packets: %lu\n", config->stats->packets);
    printf("Write    packets: %lu\n", config->stats->current_file_packets);
    printf("Free     packets: %lu\n", config->stats->packets_dropped);

    return retval;
}
