#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "hd_dns.h"
#include "../hd_file.h"
#include "../hd_print.h"

#include <rte_lcore.h>

void httpdump_dns(unsigned char *data, uint32_t len, struct timeval ts, host_t *src, host_t *dst)
{
    FILE *output = httpdump_file(rte_lcore_id());

    __print_ts(output, ts);
    fprintf(output, "DNS|");

    uint16_t questions = (uint16_t *)data[3];
    unsigned char *name = NULL;
    unsigned char *type = NULL;
    unsigned char *class = NULL;
    unsigned char *time_tl = NULL;
    unsigned char *answer = NULL;
    unsigned char *answer_len = NULL;

    if (len < DNS_HEADER_LEN)
        return;

    // Queries
    uint32_t i = DNS_HEADER_LEN, j = 0, k = questions;

    while (i < len - DNS_HEADER_LEN && k > 0)
    {
        j = i;
        while (data[j] != 0)
        {
            if (data[j] < 32 || data[j] > 126)
                data[j] = '.';
            j++;
        }

        name = data + i + 1;
        type = data + j + 2;
        class = type + 2;

        k--;
        i = j + 8;

        fprintf(output, "|%s|%s|%s|",
                name, type, class);
    }

    // Answers
    // TODO: Parse offset
    k = questions;

    while (i < len - DNS_HEADER_LEN && k > 0)
    {
        name = data + i;
        if ((*name >> 4 == 12))
        {
            name = data + (uint16_t) * (name + 1);
            j = i + 2;
        }
        else
        {
            name = data + i + 1;
            j = i;
            while (data[j] != 0)
            {
                if (data[j] < 32 || data[j] > 126)
                    data[j] = '.';
                j++;
            }
        }

        type = data + j + 2;
        class = type + 2;
        time_tl = class + 2;
        answer_len = time_tl + 4;
        answer = answer_len + 2;

        k--;
        i = j + 8;
        fprintf(output, "|%s|%d|%d|%.*s|",
                name, type, class, answer_len, answer);
    }

    __print_ip(output, src);
    fprintf(output, "|%u|", src->port);
    __print_ip(output, dst);
    fprintf(output, "|%u\n", dst->port);

}
