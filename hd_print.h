#ifndef _DPDK_HTTPDUMP_PRINT_
#define _DPDK_HTTPDUMP_PRINT_

#include "hd_pkt.h"

inline void __print_ts(FILE *output, struct timeval ts);
inline void __print_ip(FILE *output, host_t *host);

#endif //_DPDK_HTTPDUMP_PRINT_
