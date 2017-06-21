#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include "core_capture.h"

/*
 * Capture the traffic from the given port/queue tuple
 */
int capture_core(const struct core_capture_config *config)
{
    uint16_t nb_rx;
    int nb_rx_enqueued;

    struct rte_mbuf *bufs[DPDKCAP_CAPTURE_BURST_SIZE];

    printf("Core %u is capturing packets for port %u\n",
           rte_lcore_id(), config->port);

    /* Init stats */
    *(config->stats) = (struct core_capture_stats){
        .core_id = rte_lcore_id(),
        .packets = 0,
        .missed_packets = 0,
    };

    /* Run until the application is quit or killed. */
    for (;;)
    {
        /* Stop condition */
        if (unlikely(*(config->stop_condition)))
            break;

        /* Retrieve packets and put them into the ring */
        nb_rx = rte_eth_rx_burst(config->port, config->queue,
                                 bufs, DPDKCAP_CAPTURE_BURST_SIZE);
        if (likely(nb_rx > 0))
            nb_rx_enqueued = rte_ring_enqueue_bulk(config->ring, (void *)bufs,
                                                   nb_rx, NULL);
    }

    return 0;
}
