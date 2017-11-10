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

    struct rte_mbuf *bufs[HTTPDUMP_CAPTURE_BURST_SIZE];

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
        {
#ifdef DEBUG
            printf("CORE %u: Exit cause stop_condition is :%u\n",
                   config->stats->core_id,
                   *(config->stop_condition));
#endif
            break;
        }

        /* Retrieve packets and put them into the ring */
        nb_rx = rte_eth_rx_burst(config->port, config->queue,
                                 bufs, HTTPDUMP_CAPTURE_BURST_SIZE);
        if (likely(nb_rx > 0))
        {
            nb_rx_enqueued = rte_ring_enqueue_bulk(config->ring, (void *)bufs,
                                                   nb_rx, NULL);

            /* Update stats */
            if (nb_rx_enqueued != 0)
            {
                config->stats->packets += nb_rx_enqueued;
                config->stats->missed_packets += nb_rx - nb_rx_enqueued;
            }
            else
            {
                config->stats->missed_packets += nb_rx;
                /* Free whatever we can't put in the write ring */
                for (nb_rx_enqueued = 0; nb_rx_enqueued < nb_rx; nb_rx_enqueued++)
                    rte_pktmbuf_free(bufs[nb_rx_enqueued]);
            }
#ifdef DEBUG
            printf("Core %u : %lu packets, %lu missed\n",
                   rte_lcore_id(), config->stats->packets, config->stats->missed_packets);
#endif
        }
    }

    printf("------------------------\n");
    printf("Closed capture core %d (port %d)\n", rte_lcore_id(), config->port);
    printf("Capture packets: %lu\n", config->stats->packets);
    printf("Missed  Packets: %lu\n", config->stats->missed_packets);

    return 0;
}
