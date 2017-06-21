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

/*
 * Write the packets form the write ring into a pcap compressed file
 */
int write_core(const struct core_write_config *config)
{
    int to_write;
    int retval = 0;
    struct rte_mbuf *dequeued[DPDKCAP_WRITE_BURST_SIZE];
    struct rte_mbuf *mbuf;
    struct pcap_pkthdr header;

    for (;;)
    {
        if (unlikely((*(config->stop_condition)) && rte_ring_empty(config->ring)))
        {
            break;
        }

        to_write = rte_ring_dequeue_bulk(config->ring, (void *)dequeued,
                                         DPDKCAP_WRITE_BURST_SIZE, NULL);
        if (unlikely(to_write == 0))
            to_write = rte_ring_dequeue_burst(config->ring, (void *)dequeued,
                                              DPDKCAP_WRITE_BURST_SIZE, NULL);

        int i;
        for (i = 0; i < to_write; i++)
            rte_pktmbuf_free(dequeued[i]);
    }

    return retval;
}
