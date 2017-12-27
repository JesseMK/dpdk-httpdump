#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rte_lcore.h>

#include "hd_pkt.h"
#include "protocols/hd_http.h"
// #include "uthash.h"

record_t *hashtable[MAX_CORE_NUM] = {};
record_t *hashtable_old[MAX_CORE_NUM] = {};
uint64_t cache_time[MAX_CORE_NUM] = {0};

void init_hashtable(int lcore_id)
{
    hashtable[lcore_id] = NULL;
    hashtable_old[lcore_id] = NULL;
    cache_time[lcore_id] = 0;
}

void httpdump_pkt(unsigned char *data, uint32_t seq, uint16_t len, struct timeval ts, host_t *src, host_t *dst)
{

    int lcore_id = rte_lcore_id();

    if (len > HTTP_HEADER_MINLEN && (strncmp(data, "GET ", 4) == 0 ||
                                     strncmp(data, "POST ", 5) == 0 ||
                                     strncmp(data, "HEAD ", 5) == 0 ||
                                     strncmp(data, "CONNECT ", 8) == 0 ||
                                     strncmp(data, "OPTIONS ", 8) == 0 ||
                                     strncmp(data, "PUT ", 4) == 0 ||
                                     strncmp(data, "DELETE ", 7) == 0 ||
                                     strncmp(data, "TRACE ", 6) == 0 ||
                                     strncmp(data, "PATCH ", 6) == 0 ||
                                     strncmp(data, "SEARCH ", 7) == 0 ||
                                     strncmp(data, "MOVE ", 5) == 0 ||
                                     strncmp(data, "COPY ", 5) == 0 ||
                                     strncasecmp(data, "HTTP/1.", 7) == 0))
    { // New

        if (memmem(data, len, "\r\n\r\n", 4) != NULL)
        {
            httpdump_http(data, len, ts, src, dst);
        }
        else
        {
            record_t *record = calloc(1, sizeof(record_t));
            memcpy(&(record->key.src), src, sizeof(host_t));
            memcpy(&(record->key.dst), dst, sizeof(host_t));
            record_t *found = NULL;
            record_t **table = &hashtable[lcore_id];
            HASH_FIND(hh, hashtable[lcore_id], &(record->key), sizeof(hostpair_t), found);
            if (!found)
            {
                HASH_FIND(hh, hashtable_old[lcore_id], &(record->key), sizeof(hostpair_t), found);
                table = &hashtable_old[lcore_id];
            }
            if (found && found->initialseq == seq)
            { // Retransmission
                free(record);
            }
            else
            {
                if (found)
                {
                    httpdump_http(found->data, REASM_MAX_LENGTH, found->ts, src, dst);
                    HASH_DEL(*table, found);
                    free(found);
                }
                record->ts = ts;
                record->initialseq = seq;
                record->nextseq = seq + len;
                memcpy(record->data, data, len);
                HASH_ADD(hh, hashtable[lcore_id], key, sizeof(hostpair_t), record);
            }
        }
    }
    else
    {

        hostpair_t key;
        memset(&key, 0, sizeof(hostpair_t));
        memcpy(&(key.src), src, sizeof(host_t));
        memcpy(&(key.dst), dst, sizeof(host_t));

        record_t *record = NULL;
        record_t **table = &hashtable[lcore_id];
        HASH_FIND(hh, hashtable[lcore_id], &key, sizeof(hostpair_t), record);
        if (!record)
        {
            HASH_FIND(hh, hashtable_old[lcore_id], &key, sizeof(hostpair_t), record);
            table = &hashtable_old[lcore_id];
        }
        if (record)
        {
            uint32_t offset = seq - record->initialseq;
            if (seq > record->initialseq && offset < REASM_MAX_LENGTH)
            {
                memcpy(record->data + offset, data, len < REASM_MAX_LENGTH - offset ? len : REASM_MAX_LENGTH - offset);
                if (record->nextseq == seq)
                {
                    record->nextseq = seq + len;
                    if (memmem(data, len, "\r\n\r\n", 4) != NULL)
                    {
                        httpdump_http(record->data, REASM_MAX_LENGTH, record->ts, src, dst);
                        HASH_DEL(*table, record);
                        free(record);
                    }
                }
                else
                { // Out of order
                    record->nextseq = 0;
                }
            }
        }
        else
        {

            // TODO: DNS Identify
            if (src->port == 53 || dst->port == 53)
            {
                httpdump_dns(data, len, ts, src, dst);
                return;
            }
        }
    }

    uint64_t ctime = ts.tv_sec - ts.tv_sec % CACHE_FLUSH_TIME;
    if (ctime != cache_time[lcore_id])
    {
        cache_time[lcore_id] = ctime;
        if (hashtable_old[lcore_id])
        {
            record_t *record, *tmp;
            HASH_ITER(hh, hashtable_old[lcore_id], record, tmp)
            {
                httpdump_http(record->data, REASM_MAX_LENGTH, record->ts, &(record->key.src), &(record->key.dst));
                HASH_DEL(hashtable_old[lcore_id], record);
                free(record);
            }
        }
        hashtable_old[lcore_id] = hashtable[lcore_id];
        hashtable[lcore_id] = NULL;
    }
}
