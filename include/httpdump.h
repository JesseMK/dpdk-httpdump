#ifndef _DPDK_HTTPDUMP_
#define _DPDK_HTTPDUMP_

#include <argp.h>

struct arguments
{
    char *l_flag;
    char *c_flag;
    char *n_flag;
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
const char *argp_program_version = "DPDK-HTTPDUMP 0.2";
const char *argp_program_bug_address = "m.k.jessie@sjtu.edu.cn";
static char doc[] = "A DPDK-based http packet analysis tool";
static char args_doc[] = "";

static struct argp_option options[] = {};

#endif //_HTTPDUMP_
