#ifndef _DPDK_HTTPDUMP_FILE_
#define _DPDK_HTTPDUMP_FILE_

#include <stdint.h>

#define MAX_CORE_NUM 64
#define BASIC_PATH "/dev/shm/dpdk-httpdump/"

uint32_t rotate_interval;
uint32_t rotate_last[MAX_CORE_NUM];
FILE *fp[MAX_CORE_NUM];

inline FILE *httpdump_file(int lcore_id);

#endif //_DPDK_HTTPDUMP_FILE_
