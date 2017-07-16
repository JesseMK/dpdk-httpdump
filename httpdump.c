#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <inttypes.h>

#include "bypasscap.h"
#include "core_write.h"
#include "core_capture.h"

struct arguments
{
    uint64_t portmask;
    unsigned long nb_mbufs;
    char *num_rx_desc_str_matrix;
    unsigned long per_port_c_cores;
    unsigned long num_w_cores;
    unsigned long snaplen;
    unsigned long rotate_seconds;
};

struct arguments arguments;

static unsigned int portlist[64];
static unsigned int nb_ports;

static struct core_write_stats *cores_stats_write_list;
static struct core_capture_stats *cores_stats_capture_list;

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .mq_mode = ETH_MQ_RX_RSS,
        .max_rx_pkt_len = ETHER_MAX_LEN,
        // .rx_adv_conf.rss_conf.rss_hf = ETH_RSS_TCP,
    }};

void usage(char *progname)
{
    printf("Usage: %s [-c <per_port_c_cores>] [-w <num_w_cores>]  [-p <portmask>] [-r <rotate_seconds>]\n\
            -c      Number of cores to capture packages on each port\n\
            -w      Number of cores to analysis traffic\n\
            -p      An hexadecimal bit mask of the port to listen on\n\
            -r      Every X seconds to generate new files for writing\n",
           progname);
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static int port_init(
    uint8_t port,
    const uint16_t rx_rings,
    unsigned int num_rxdesc,
    struct rte_mempool *mbuf_pool)
{

    struct rte_eth_conf port_conf = port_conf_default;
    struct rte_eth_dev_info dev_info;
    int retval;
    uint16_t q;

    /* Get the device info */
    rte_eth_dev_info_get(port, &dev_info);

    /* Check if the requested number of queue is valid */
    if (rx_rings > dev_info.max_rx_queues)
    {
        printf("Port %d can only handle up to %d queues (%d "
               "requested).\n",
               port, dev_info.max_rx_queues, rx_rings);
        return -1;
    }

    /* Check if the number of requested RX descriptors is valid */
    if (num_rxdesc > dev_info.rx_desc_lim.nb_max ||
        num_rxdesc < dev_info.rx_desc_lim.nb_min ||
        num_rxdesc % dev_info.rx_desc_lim.nb_align != 0)
    {
        printf("Port %d cannot be configured with %d RX "
               "descriptors per queue (min:%d, max:%d, align:%d)\n",
               port, num_rxdesc, dev_info.rx_desc_lim.nb_min,
               dev_info.rx_desc_lim.nb_max, dev_info.rx_desc_lim.nb_align);
        return -1;
    }

    /* Configure multiqueue (Activate Receive Side Scaling on UDP/TCP fields) */
    if (rx_rings > 1)
    {
        port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
        port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
        port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_TCP;
    }

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, 0, &port_conf);
    if (retval)
    {
        printf("rte_eth_dev_configure(...): %s\n",
               rte_strerror(-retval));
        return retval;
    }

    /* Allocate and set up RX queues. */
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, num_rxdesc,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval)
        {
            printf("rte_eth_rx_queue_setup(...): %s\n",
                   rte_strerror(-retval));
            return retval;
        }
    }

    /* Stats bindings (if more than one queue) */
    if (dev_info.max_rx_queues > 1)
    {
        for (q = 0; q < rx_rings; q++)
        {
            retval = rte_eth_dev_set_rx_queue_stats_mapping(port, q, q);
            if (retval)
            {
                printf("rte_eth_dev_set_rx_queue_stats_mapping(...):"
                       " %s\n",
                       rte_strerror(-retval));
                printf("The queues statistics mapping failed. The "
                       "displayed queue statistics are thus unreliable.\n");
            }
        }
    }

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    printf("Port %u: MAC=%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
           ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", RXdesc/queue=%d\n",
           (unsigned)port,
           addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
           addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5],
           num_rxdesc);

    return 0;
}

/*
 * Handles signals
 */
static volatile uint8_t quit_signal = 0;
static void signal_handler(int sig)
{
    printf("Caught signal %s on core %u%s\n",
           strsignal(sig), rte_lcore_id(),
           rte_get_master_lcore() == rte_lcore_id() ? " (MASTER CORE)" : "");
    quit_signal = 1;
}

int main(int argc, char *argv[])
{
    // catch ctrl-c so we can uninit on exit
    signal(SIGINT, signal_handler);

    struct core_capture_config *cores_config_capture_list;
    struct core_write_config *cores_config_write_list;
    unsigned int lcoreid_list[MAX_LCORES];
    unsigned int nb_lcores;
    struct rte_mempool *mbuf_pool;
    unsigned int i, j;
    unsigned int port_id;
    unsigned int required_cores;
    unsigned int core_index;
    int result;
    char ring_name[SIZE];
    char mempool_name[SIZE];

    /* Initialize the Environment Abstraction Layer (EAL). */
    init_dpdk_eal();

    /* Parse arguments */
    arguments = (struct arguments){
        .nb_mbufs = NUM_MBUFS_DEFAULT,
        .num_rx_desc_str_matrix = NULL,
        .per_port_c_cores = 3,
        .num_w_cores = 6, //need to be <= per_port_c_cores*ports
        .snaplen = PCAP_SNAPLEN_DEFAULT,
        .portmask = 0x3,
        .rotate_seconds = 600,
    };

    int c;
    while ((c = getopt(argc, argv, "hc:w:p:r:")) != -1)
    {
        switch (c)
        {
        case 'c':
            arguments.per_port_c_cores = atoi(optarg);
            break;
        case 'w':
            arguments.num_w_cores = atoi(optarg);
            break;
        case 'p':
            arguments.portmask = strtol(optarg, NULL, 16);
            break;
        case 'r':
            arguments.rotate_seconds = atoi(optarg);
            break;
        case 'h':
            usage("dpdk-httpdump");
            return 0;
        }
    }

    /* Check if at least one port is available */
    if (rte_eth_dev_count() == 0)
        rte_exit(EXIT_FAILURE, "Error: No port available.\n");

    /* Fills in the number of rx descriptors matrix */
    unsigned long *num_rx_desc_matrix = calloc(rte_eth_dev_count(), sizeof(int));
    if (arguments.num_rx_desc_str_matrix != NULL &&
        parse_matrix_opt(arguments.num_rx_desc_str_matrix,
                         num_rx_desc_matrix, rte_eth_dev_count()) < 0)
    {
        rte_exit(EXIT_FAILURE, "Invalid RX descriptors matrix.\n");
    }

    /* Creates the port list */
    nb_ports = 0;
    for (i = 0; i < 64; i++)
    {
        if (!((uint64_t)(1ULL << i) & arguments.portmask))
            continue;
        if (i < rte_eth_dev_count())
            portlist[nb_ports++] = i;
        else
            printf("Warning: port %d is in portmask, "
                   "but not enough ports are available. Ignoring...\n",
                   i);
    }
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "Error: Found no usable port. Check portmask "
                               "option.\n");

    printf("Using %u ports to listen on\n", nb_ports);

    /* Checks core number */
    required_cores = (1 + nb_ports * arguments.per_port_c_cores + arguments.num_w_cores);
    if (rte_lcore_count() < required_cores)
    {
        rte_exit(EXIT_FAILURE, "Assign at least %d cores to dpdkcap.\n",
                 required_cores);
    }
    printf("Using %u cores out of %d allocated\n",
           required_cores, rte_lcore_count());

    /* Creates a new mempool in memory to hold the mbufs. */
    // mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", arguments.nb_mbufs,
    //                                     MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
    //                                     rte_socket_id());
    // if (mbuf_pool == NULL)
    //     rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Core index */
    core_index = rte_get_next_lcore(-1, 1, 0);

    /* Init stats list */
    cores_stats_write_list =
        malloc(sizeof(struct core_write_stats) * arguments.num_w_cores);
    cores_stats_capture_list =
        malloc(sizeof(struct core_capture_stats) * arguments.per_port_c_cores * nb_ports);

    /* Init config lists */
    cores_config_write_list =
        malloc(sizeof(struct core_write_config) * arguments.num_w_cores);
    cores_config_capture_list =
        malloc(sizeof(struct core_capture_config) * arguments.per_port_c_cores * nb_ports);

    nb_lcores = 0;
    /* Writing cores */
    for (i = 0; i < arguments.num_w_cores; i++)
    {
#ifdef DEBUG
        printf("config core %d to write...\n", i);
#endif

        //Initialize buffer for writing to disk
        snprintf(ring_name, SIZE, RX_RING, i);
        write_ring[i] = rte_ring_create(ring_name,
                                        rte_align32pow2(arguments.nb_mbufs), rte_socket_id(), 0);

        //Configure writing core
        struct core_write_config *config = &(cores_config_write_list[i]);
        config->ring = write_ring[i];
        config->stop_condition = &quit_signal;
        config->stats = &(cores_stats_write_list[i]);
        config->snaplen = arguments.snaplen;
        config->rotate_seconds = arguments.rotate_seconds;

        //Launch writing core
        if (rte_eal_remote_launch((lcore_function_t *)write_core,
                                  config, core_index) < 0)
            rte_exit(EXIT_FAILURE, "Could not launch writing process on lcore %d.\n",
                     core_index);

        //Add the core to the list
        lcoreid_list[nb_lcores] = core_index;
        nb_lcores++;

        core_index = rte_get_next_lcore(core_index, SKIP_MASTER, 0);
    }

    /* For each port */
    for (i = 0; i < nb_ports; i++)
    {
        printf("config port %d to capture...\n", i);

        snprintf(mempool_name, SIZE, MP_NAME, i);
        mbuf_pool = rte_mempool_lookup(mempool_name);
        if (mbuf_pool == NULL)
        {
            /* create mempool */
            mbuf_pool = rte_pktmbuf_pool_create(mempool_name,
                                                arguments.nb_mbufs,
                                                MBUF_CACHE_SIZE, 0,
                                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                                rte_socket_id());
            if (mbuf_pool == NULL)
            {
                // cleanup_rings();
                rte_exit(EXIT_FAILURE,
                         "Mempool creation failed: %s\n",
                         rte_strerror(rte_errno));
            }
        }

        port_id = portlist[i];

        /* Port init */
        int retval = port_init(
            port_id,
            arguments.per_port_c_cores,
            (num_rx_desc_matrix[i] != 0) ? num_rx_desc_matrix[i] : RX_DESC_DEFAULT,
            mbuf_pool);
        if (retval)
        {
            rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n", port_id);
        }

        /* Capturing cores */
        for (j = 0; j < arguments.per_port_c_cores; j++)
        {
            //Configure capture core
            struct core_capture_config *config =
                &(cores_config_capture_list[i * arguments.per_port_c_cores + j]);
            config->ring = write_ring[(i * arguments.per_port_c_cores + j) % arguments.num_w_cores]; //need wirte core <= cap core
            config->stop_condition = &quit_signal;
            config->stats =
                &(cores_stats_capture_list[i * arguments.per_port_c_cores + j]);
            config->port = port_id;
            config->queue = j;
            //Launch capture core
            if (rte_eal_remote_launch((lcore_function_t *)capture_core,
                                      config, core_index) < 0)
                rte_exit(EXIT_FAILURE, "Could not launch capture process on lcore "
                                       "%d.\n",
                         core_index);

            printf("Capture Core %d -> Write Core %d\n", core_index,
                   ((i * arguments.per_port_c_cores + j) % arguments.num_w_cores));
            //Add the core to the list
            lcoreid_list[nb_lcores] = core_index;
            nb_lcores++;

            core_index = rte_get_next_lcore(core_index, SKIP_MASTER, 0);
        }

        /* Start the port once everything is ready to capture */
        retval = rte_eth_dev_start(port_id);
        if (retval)
            rte_exit(EXIT_FAILURE, "Cannot start port %" PRIu8 "\n", port_id);
#ifdef DEBUG
        else
            printf("Port %d: Up\n", port_id);
#endif
    }

    //Wait for all the cores to complete and exit
    printf("Waiting for all cores to exit\n");
    for (i = 0; i < nb_lcores; i++)
    {
        result = rte_eal_wait_lcore(lcoreid_list[i]);
        if (result < 0)
        {
            printf("Core %d did not stop correctly.\n",
                   lcoreid_list[i]);
        }
    }

    //Finalize
    free(cores_stats_write_list);
    free(cores_stats_capture_list);
    free(cores_config_write_list);
    free(cores_config_capture_list);
    free(num_rx_desc_matrix);

    return 0;
}
