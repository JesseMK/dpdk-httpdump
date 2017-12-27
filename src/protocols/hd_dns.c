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

    uint16_t questions = (uint16_t *)data[4];
    unsigned char *name = NULL;
    unsigned char *type = NULL;
    unsigned char *class = NULL;
    unsigned char *time_tl = NULL;
    unsigned char *answer = NULL;
    unsigned char *answer_len = NULL;

    if (len < DNS_HEADER_LEN)
        return;

    FILE *output = httpdump_file(rte_lcore_id());

    fprintf(output, "\nraw_data: %x", data[0]);
    for (int i = 1; i < len; i++)
    {
        fprintf(output, "%x", data[i]);
    }
    fprintf(output, "\n");

    __print_ts(output, ts);
    fprintf(output, "|DNS");

    // Queries
    uint32_t i = DNS_HEADER_LEN, j = 0, k = questions;

    while (i < len && k > 0)
    {
        j = i;
        while (data[j] != 0)
        {
            if (data[j] < 32 || data[j] > 126)
                data[j] = '.';
            j++;
        }
        j++;

        name = data + i + 1;
        type = data + j;
        class = type + 2;

        k--;
        i = j + 4;

        fprintf(output, "|name:%s|type:%u|class:%u|",
                name, (uint16_t)*type, (uint16_t) * class);
    }

    // Answers
    // TODO: Parse offset
    // k = questions;

    // while (i < len && k > 0)
    // {
    //     name = data + i;
    //     if ((*name >> 4 == 12))
    //     {
    //         name = data + (uint16_t) * (name + 1);
    //         j = i + 2;
    //     }
    //     else
    //     {
    //         name = data + i + 1;

    //         j = i;
    //         while (data[j] != 0)
    //         {
    //             if (data[j] < 32 || data[j] > 126)
    //                 data[j] = '.';
    //             j++;
    //             if (j > len)
    //                 return;
    //         }
    //         j++;
    //     }

    //     type = data + j;
    //     class = type + 2;
    //     time_tl = class + 2;
    //     answer_len = time_tl + 4;
    //     answer = answer_len + 2;

    //     k--;
    //     i = j + 10 + answer_len;
    //     fprintf(output, "|%s|%u|%u|%.*s|",
    //             name, (uint16_t)*type, (uint16_t) * class, (uint16_t)*answer_len, answer);
    // }

    __print_ip(output, src);
    fprintf(output, "|%u|", src->port);
    __print_ip(output, dst);
    fprintf(output, "|%u\n", dst->port);
}
