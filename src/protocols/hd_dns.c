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

    uint16_t questions = *(uint16_t *)(data + 5);
    unsigned char *name = NULL;
    unsigned char *type = NULL;
    unsigned char *class = NULL;
    unsigned char *time_tl = NULL;
    unsigned char *answer = NULL;
    unsigned char *answer_len = NULL;

    if (len < DNS_MINLEN || questions < 1)
        return;

    uint32_t pos = DNS_HEADER_LEN;

    if (data[pos] < 1 || data[pos] > 10)
        return;

    while (data[pos] != 0)
    {
        pos += data[pos] + 1;
        if (pos > len || data[pos] < 0 || data[pos] > 10 || len - pos < 5)
            return;
    }

    FILE *output = httpdump_file(rte_lcore_id());

    fprintf(output, "\nraw_data: %02x ", data[0]);
    for (int i = 1; i < len; i++)
    {
        fprintf(output, "%02x ", data[i]);
    }
    fprintf(output, "\n");

    __print_ts(output, ts);
    fprintf(output, "|DNS|Queries:%u", questions);

    // Queries
    uint32_t i = DNS_HEADER_LEN, j = 0, k = 0, q = questions, field_len = 0;

    while (i < len && q > 0)
    {
        // NAME
        if (((data[i] & 0xf0) >> 4) == 0xc)
        {
            j = (uint16_t)(data[i + 1] & 0xf);
            fprintf(output, "|flag:%02x|pos:%u|offset:%u", (data[i] & 0xf0) >> 4, i, j);
        }
        else
            j = i;

        name = data + j + 1;

        if ((data[j] < 1 || data[j] > 10) && data[j] != 0x2e)
        {
            fprintf(output, "|ERROR1@%u:%02x\n", j, data[j]);
            return;
        }
        if (i == j)
        {
            while (data[j] != 0 && j < len)
            {
                field_len = data[j];
                data[j] = '.';
                k = j + 1;
                while (k < j + field_len)
                {
                    if (data[k] < 32 || data[k] > 126)
                    {
                        fprintf(output, "|ERROR2@%u:%02x\n", k, data[k]);
                        return;
                    }
                    k++;
                }

                j = k + 1;
                if (j > len || data[j] > 10 || len - j < 5)
                {
                    fprintf(output, "|ERROR3@%u:%02x\n", j, data[j]);
                    return;
                }
            }
        }

        type = (j > i) ? data + j : data + i + 4;
        class = type + 2;

        q--;
        i = j + 5;

        fprintf(output, "|name:%s|type:%u|class:%u",
                name, *(uint16_t *)type, *(uint16_t *)class);
    }

    if (data[2] > 0x7f)
    {
        // Answers
        // TODO: Parse offset
        q = *(uint16_t *)(data + 7);

        fprintf(output, "|RSP:%u", q);

        while (i < len && q > 0)
        {
            // NAME
            if (((data[i] & 0xf0) >> 4) == 0xc)
                j = (uint16_t)(data[i + 1] & 0xf);
            else
                j = i;

            fprintf(output, "|flag:%02x|pos:%u|offset:%u", (data[i] & 0xf0) >> 4, i, j);

            name = data + j + 1;

            if ((data[j] < 1 || data[j] > 10) && data[j] != 0x2e)
            {
                fprintf(output, "|ERROR1@%u:%02x\n", j, data[j]);
                return;
            }

            if (i == j)
            {
                while (data[j] != 0 && j < len)
                {
                    field_len = data[j];
                    data[j] = '.';
                    k = j + 1;
                    while (k < j + field_len)
                    {
                        if (data[k] < 32 || data[k] > 126)
                        {
                            fprintf(output, "|ERROR2@%u:%02x\n", k, data[k]);
                            return;
                        }
                        k++;
                    }

                    j = k + 1;
                    if (j > len || data[j] > 10 || len - j < 5)
                    {
                        fprintf(output, "|ERROR3@%u:%02x\n", j, data[j]);
                        return;
                    }
                }
            }

            type = (j > i) ? data + j : data + i + 2;
            class = type + 2;
            time_tl = class + 2;
            answer_len = time_tl + 4;
            answer = answer_len + 2;

            q--;
            i = answer + *(uint16_t *)(answer_len + 1);

            fprintf(output, "|name:%s|type:%u|class:%u|len:%u|answer:%*s",
                    name, *(uint16_t *)type, *(uint16_t *)class,
                    *(uint16_t *)(answer_len + 1), *(uint16_t *)(answer_len + 1), answer);
        }
    }
    fprintf(output, "|");
    __print_ip(output, src);
    fprintf(output, "|%u|", src->port);
    __print_ip(output, dst);
    fprintf(output, "|%u\n", dst->port);
}
