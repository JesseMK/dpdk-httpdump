/*
 * @Author: JesseMK
 * @Date: 2017-10-19 16:28:14
 * @Last Modified by: JesseMK
 * @Last Modified time: 2017-10-19 21:02:28
 */

#ifndef _DPDK_HTTPDUMP_CONFIG_
#define _DPDK_HTTPDUMP_CONFIG_

#define DPDK_MAX_LCORE 128

#define SIZE 256
#define BURST_SIZE 32
#define NUM_MBUFS 8191
#define RX_RING_SIZE 128
#define TX_RING_SIZE 512
#define MBUF_CACHE_SIZE 256
#define RX_RING "cap_ring_%d"
#define MP_NAME "httpdump_pool_%d"

extern int dpdk_argc;
extern char **dpdk_argv;

int parse_matrix_opt(char *arg, unsigned long *matrix,
                            unsigned long max_len);

#endif /* ifndef _DPDK_HTTPDUMP_CONFIG_ */
