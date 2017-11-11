#ifndef _DPDK_HTTPDUMP_PKT_
#define _DPDK_HTTPDUMP_PKT_

#include <stdint.h>

#include "uthash.h"

#define MAX_CORE_NUM 32
#define REASM_MAX_LENGTH 65536
#define CACHE_FLUSH_TIME 16

typedef struct
{
    uint8_t ipver;
    uint32_t ip4;
    uint64_t ip6h;
    uint64_t ip6l;
    uint16_t port;
} host_t;

typedef struct
{
    host_t src;
    host_t dst;
} hostpair_t;

typedef struct
{
    hostpair_t key;
    struct timeval ts;
    uint32_t initialseq;
    uint32_t nextseq;
    char data[REASM_MAX_LENGTH];
    UT_hash_handle hh;
} record_t;

extern record_t *hashtable[MAX_CORE_NUM];
extern record_t *hashtable_old[MAX_CORE_NUM];
extern uint64_t cache_time[MAX_CORE_NUM];

void init_hashtable(int lcore_id);
void httpdump_pkt(unsigned char *data, uint32_t seq, uint16_t len, struct timeval ts, host_t *src, host_t *dst);

#endif //_DPDK_HTTPDUMP_PKT_
