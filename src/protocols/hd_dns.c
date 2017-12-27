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

    // fprintf(output, "\nraw_data: %02x ", data[0]);
    // for (int i = 1; i < len; i++)
    // {
    //     fprintf(output, "%02x ", data[i]);
    // }
    // fprintf(output, "\n");

    __print_ts(output, ts);
    fprintf(output, "|DNS|%u", questions);

    // Queries
    uint32_t i = DNS_HEADER_LEN, j = 0, k = 0, q = questions, field_len = 0;

    while (i < len && q > 0)
    {
        // NAME
        if (((data[i] & 0xf0) >> 4) == 0xc)
        {
            j = (uint16_t)(data[i + 1] & 0xf);
            // fprintf(output, "|flag:%02x|pos:%u|offset:%u", (data[i] & 0xf0) >> 4, i, j);
        }
        else
            j = i;

        name = data + j + 1;

        // Untreated name
        if (i == j)
        {
            if (data[j] < 1 || data[j] > 10)
            {
                // fprintf(output, "|ERROR1@%u:%02x\n", j, data[j]);
                return;
            }
            while (data[j] != 0 && j < len)
            {
                if (data[j] > 10 || len - j < 5)
                {
                    // fprintf(output, "|ERROR3@%u:%02x\n", j, data[j]);
                    return;
                }
                field_len = data[j];
                data[j] = '.';
                j += field_len + 1;
            }
            j++;
        }

        type = (j > i) ? data + j : data + i + 4;
        class = type + 2;

        q--;
        i = (j > i) ? j + 4 : i + 8;

        // TODO: Big end
        fprintf(output, "|%s|%u|%u",
                name, *(uint8_t *)(type + 1), *(uint8_t *)(class + 1));
    }

    // Answers
    if (data[2] > 0x7f)
    {
        // TODO: Parse offset
        q = *(uint16_t *)(data + 7);

        // fprintf(output, "|RSP:%u|pos:%u", q, i);

        while (i < len && q > 0)
        {
            // NAME
            if (((data[i] & 0xf0) >> 4) == 0xc)
                j = (uint16_t)(data[i + 1] & 0xf);
            else
                j = i;

            // fprintf(output, "|flag:%02x|pos:%u|offset:%u", (data[i] & 0xf0) >> 4, i, j);

            name = data + j + 1;

            // Untreated name
            if (i == j)
            {
                if (data[j] < 1 || data[j] > 10)
                {
                    // fprintf(output, "|ERROR1@%u:%02x\n", j, data[j]);
                    return;
                }
                while (data[j] != 0 && j < len)
                {
                    if (data[j] > 10 || len - j < 5)
                    {
                        // fprintf(output, "|ERROR3@%u:%02x\n", j, data[j]);
                        return;
                    }
                    field_len = data[j];
                    data[j] = '.';
                    j += field_len + 1;
                }
                j++;
            }

            type = (j > i) ? data + j : data + i + 2;
            class = type + 2;
            time_tl = class + 2;
            answer_len = time_tl + 4;
            answer = answer_len + 2;

            q--;
            // i -> answer
            i = ((j > i) ? j : i + 2) + 10;

            // TODO: Big end
            fprintf(output, "|%s|%u|%u|%u",
                    name, *(uint8_t *)(type + 1), *(uint8_t *)(class + 1),
                    *(uint8_t *)(answer_len + 1));

            // TODO:ipv6
            if (*(uint8_t *)(answer_len + 1) == 4)
            {
                struct in_addr addr;
                addr.s_addr = htobe32(answer);
                fprintf(output, "|%s", inet_ntoa(addr));
            }
            else
            {
                // Answer is not a IP
                if (((data[i] & 0xf0) >> 4) == 0xc)
                {
                    j = (uint16_t)(data[i + 1] & 0xf);
                    // fprintf(output, "|flag:%02x|pos:%u|offset:%u", (data[i] & 0xf0) >> 4, i, j);
                }
                else
                    j = i;

                answer = data + j + 1;

                // Untreated name
                if (i == j)
                {
                    while (data[j] != 0 && j < len)
                    {
                        if (data[j] < 32 || data[j] > 126)
                            data[j] = '.';
                        j++;
                    }
                }

                fprintf(output, "|%.*s",
                        *(uint8_t *)(answer_len + 1), answer);
            }

            i += *(uint8_t *)(answer_len + 1);
        }
    }
    fprintf(output, "|");
    __print_ip(output, src);
    fprintf(output, "|%u|", src->port);
    __print_ip(output, dst);
    fprintf(output, "|%u\n", dst->port);
}
