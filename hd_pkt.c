#ifndef _DPDK_HTTPDUMP_FILE_
#define _DPDK_HTTPDUMP_FILE_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rte_lcore.h>

#include "hd_pkt.h"
#include "hd_http.h"
#include "uthash.h"

record_t *hashtable[MAX_CORE_NUM] = {};
record_t *hashtable_old[MAX_CORE_NUM] = {};
uint64_t cache_time[MAX_CORE_NUM] = {0};

inline FILE *httpdump_file(int lcore_id)
{
    struct timeval now;
    if (fp[lcore_id] == NULL || rotate_interval > 0)
    {
        gettimeofday(&now, NULL);
        if (rotate_interval > 0)
        {
            now.tv_sec = now.tv_sec - now.tv_sec % rotate_interval;
        }
    }
    if (rotate_interval > 0 && now.tv_sec != rotate_last[lcore_id])
    {
        rotate_last[lcore_id] = now.tv_sec;
        if (fp[lcore_id] != NULL)
        {
            fclose(fp[lcore_id]);
            fp[lcore_id] = NULL;
        }
    }
    if (fp[lcore_id] == NULL)
    {
        char filename[64];
        char timestring[16];
        struct tm *now_tm = localtime(&(now.tv_sec));
        strftime(timestring, sizeof timestring, "%Y%m%d-%H%M%S", now_tm);
        snprintf(filename, sizeof filename, "%s%s-core%d.log", BASIC_PATH, timestring, lcore_id);
        fprintf(stdout, "Opening log file: %s\n", filename);
        fp[lcore_id] = fopen(filename, "a");
        if (fp[lcore_id] == NULL)
        {
            fprintf(stderr, "Error: Unable to open file for appending: %s\n", filename);
            return stdout;
        }
    }
    return fp[lcore_id];
}

#endif //_HTTPDUMP_FILE_
