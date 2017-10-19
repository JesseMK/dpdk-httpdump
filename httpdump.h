#ifndef _DPDK_HTTPDUMP_
#define _DPDK_HTTPDUMP_

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

struct capture_stats
{
    uint64_t dequeue_pkts;
    uint64_t tx_pkts;
    uint64_t freed_pkts;
} stats;

/* ARGP */
const char *argp_program_version = "DPDK-HTTPDUMP 1.0";
const char *argp_program_bug_address = "m.k.jessie@sjtu.edu.cn";
static char doc[] = "A DPDK-based http packet analysis tool";
static char args_doc[] = "";

static struct argp_option options[] = {
    {"output", 'o', "FILE", 0,
     "Output FILE template (don't add the "
     "extension). Use \"" DPDKCAP_OUTPUT_TEMPLATE_TOKEN_CORE_ID "\" for "
     "inserting the lcore id into the file name (automatically added if not "
     "used). (default: " DPDKCAP_OUTPUT_TEMPLATE_DEFAULT ")",
     0},
    {"statistics", 'S', 0, 0, "Print statistics every few seconds.", 0},
    {"num-mbuf", 'm', "NB_MBUF", 0,
     "Total number of memory buffer used to "
     "store the packets. Optimal values, in terms of memory usage, are powers "
     "of 2 minus 1 (2^q-1) (default: " STR(NUM_MBUFS_DEFAULT) ")",
     0},
    {"per_port_c_cores", 'c', "NB_CORES_PER_PORT", 0,
     "Number of cores per "
     "port used for capture (default: 1)",
     0},
    {"num_w_cores", 'w', "NB_CORES", 0,
     "Total number of cores used for "
     "writing (default: 1).",
     0},
    {"rx_desc", 'd', "RX_DESC_MATRIX", 0,
     "This option can be used to "
     "override the default number of RX descriptors configured for all queues "
     "of each port (" STR(RX_DESC_DEFAULT) "). RX_DESC_MATRIX can have "
                                           "multiple formats:\n"
                                           "- A single positive value, which will simply replace the default "
                                           " number of RX descriptors,\n"
                                           "- A list of key-values, assigning a configured number of RX "
                                           "descriptors to the given port(s). Format: \n"
                                           "  <matrix>   := <key>.<nb_rx_desc> { \",\" <key>.<nb_rx_desc> \",\" "
                                           "...\n"
                                           "  <key>      := { <interval> | <port> }\n"
                                           "  <interval> := <lower_port> \"-\" <upper_port>\n"
                                           "  Examples: \n"
                                           "  512               - all ports have 512 RX desc per queue\n"
                                           "  0.256, 1.512      - port 0 has 256 RX desc per queue,\n"
                                           "                      port 1 has 512 RX desc per queue\n"
                                           "  0-2.256, 3.1024   - ports 0, 1 and 2 have 256 RX desc per "
                                           " queue,\n"
                                           "                      port 3 has 1024 RX desc per queue.",
     0},
    {"rotate_seconds", 'G', "T", 0,
     "Create a new set of files every T "
     "seconds. Use strftime formats within the output file template to rename "
     "each file accordingly.",
     0},
    {"limit_file_size", 'C', "SIZE", 0,
     "Before writing a packet, check "
     "whether the target file excess SIZE bytes. If so, creates a new file. "
     "Use \"" DPDKCAP_OUTPUT_TEMPLATE_TOKEN_FILECOUNT "\" within the output "
     "file template to index each new file.",
     0},
    {"portmask", 'p', "PORTMASK", 0, "Ethernet ports mask (default: 0x1).", 0},
    {"snaplen", 's', "LENGTH", 0, "Snap the capture to snaplen bytes "
                                  "(default: 65535).",
     0},
    {"logs", 700, "FILE", 0, "Writes the logs into FILE instead of "
                             "stderr.",
     0},
    {"no-compression", 701, 0, 0, "Do not compress capture files.", 0},
    {0}};

static inline void init_dpdk_eal(char prgo_name[], int argv, char **argp)
{
    int diag;
    // int argv = 3;

    // char prgo_name[] = "dpdk-httpdump";
    // char l_flag[] = "-l0-3";
    // char c_flag[] = "-c1";
    // char n_flag[] = "-n6";
    // char *argp[argv];

    argp[0] = prgo_name;
    argp[1] = l_flag;
    argp[2] = c_flag;
    argp[2] = n_flag;

    diag = rte_eal_init(argv, argp);
    if (diag < 0)
        rte_panic("Cannot init EAL\n");
}

#endif //_HTTPDUMP_
