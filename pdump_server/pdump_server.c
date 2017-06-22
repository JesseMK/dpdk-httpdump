#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_pdump.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

uint64_t dequeue_pkts;
volatile uint8_t quit_signal;

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_rx_pkt_len = ETHER_MAX_LEN}};

static inline void init_dpdk_eal(void)
{
    int diag;

    char prgo_name[] = "pdump-server";
    char l_flag[] = "-l5";
    char c_flag[] = "-c1";
    char n_flag[] = "-n4";
    char *argp[4];

    argp[0] = prgo_name;
    argp[1] = l_flag;
    argp[2] = c_flag;
    argp[3] = n_flag;

    diag = rte_eal_init(3, argp);
    if (diag < 0)
        rte_panic("Cannot init EAL\n");
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;

    if (port >= rte_eth_dev_count())
        return -1;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                                        rte_eth_dev_socket_id(port), NULL);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
           " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
           (unsigned)port,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    return 0;
}

static void
signal_handler(int sig_num)
{
    if (sig_num == SIGINT)
    {
        printf("\n\nSignal %d received, preparing to exit...\n",
               sig_num);
        rte_pdump_uninit();

        quit_signal = 1;
    }
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and do nothing.
 */
static inline void
lcore_main(void)
{
    printf("Start to receive packets...\n");
    const uint8_t nb_ports = rte_eth_dev_count();
    uint8_t port;

    /*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
    for (port = 0; port < nb_ports; port++)
        if (rte_eth_dev_socket_id(port) > 0 &&
            rte_eth_dev_socket_id(port) !=
                (int)rte_socket_id())
            printf("WARNING, port %u is on remote NUMA node to "
                   "polling thread.\n\tPerformance will "
                   "not be optimal.\n",
                   port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
           rte_lcore_id());

    /* Run until the application is quit or killed. */
    while (!quit_signal)
    {
        /*
		 * Receive packets on a port and forward them on the paired
		 * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
		 */
        for (port = 0; port < nb_ports; port++)
        {

            /* Get burst of RX packets, from first port of pair. */
            struct rte_mbuf *bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                                                    bufs, BURST_SIZE);
            dequeue_pkts += nb_rx;

            if (unlikely(nb_rx == 0))
                continue;
#ifdef DEBUG
            printf("Receive %u packets on prot %u\n", nb_rx, port);
#endif

            uint16_t buf;
            for (buf = 0; buf < nb_rx; buf++)
                rte_pktmbuf_free(bufs[buf]);
        }
    }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(void)
{
    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint8_t portid;

    /* Initialize the Environment Abstraction Layer (EAL). */
    init_dpdk_eal();

    // Initialize PDUMP
    rte_pdump_init(NULL);

    // catch ctrl-c so we can uninit pdump on exit
    signal(SIGINT, signal_handler);

    nb_ports = rte_eth_dev_count();
    printf("nb_ports: %u\n", nb_ports);

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    for (portid = 0; portid < nb_ports; portid++)
        if (port_init(portid, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n",
                     portid);

    /* Call lcore_main on the master core only. */
    lcore_main();

    printf("\nPackets Dequeued:			%" PRIu64 "\n", dequeue_pkts);

    return 0;
}
