#ifndef _DPDK_HTTPDUMP_FILE_
#define _DPDK_HTTPDUMP_FILE_

#include <stdio.h>
#include <stdint.h>

#define MAX_CORE_NUM 64
#define BASIC_PATH "log/"

extern uint32_t rotate_interval;
extern uint32_t rotate_last[MAX_CORE_NUM];
extern FILE *fp[MAX_CORE_NUM];

FILE *httpdump_file(int lcore_id);

#endif //_DPDK_HTTPDUMP_FILE_
