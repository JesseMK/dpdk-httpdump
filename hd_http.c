#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "hd_http.h"
#include "hd_file.h"
#include "hd_print.h"

#include <rte_lcore.h>

void httpdump_http(unsigned char *data, uint32_t len, struct timeval ts, host_t *src, host_t *dst)
{

    unsigned char *type = NULL;
    unsigned char *domain = NULL;
    unsigned char *path = NULL;
    unsigned char *conttype = NULL;
    unsigned char *contlen = NULL;
    unsigned char *referer = NULL;
    unsigned char *useragent = NULL;

    if (len < HTTP_HEADER_MINLEN)
        return;

    if (strncmp(data, "GET ", 4) == 0 ||
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
        strncmp(data, "COPY ", 5) == 0)
    { // HTTP Request

        uint32_t i = 0, j = 0;

        while (i < len - HTTP_FIELD_MINLEN && (!type || !domain || !path || !conttype || !contlen || !referer || !useragent))
        {

            j = i;
            while (j < len - 1 && data[j] != '\r' && data[j] != '\n' && data[j] != 0)
            {
                if (data[j] < 32 || data[j] > 126)
                    data[j] = '_';
                j++;
            }
            data[j] = 0;

            if (i == 0)
            { // First line
                if (j < HTTP_REQUEST_LINE_MINLEN)
                    return;
                if (data[j - 9] != ' ')
                {
                    return;
                }
                else
                {
                    data[j - 9] = 0;
                }
                unsigned char *space = strchr(data, ' ');
                if (space == NULL)
                    return;
                *space = 0;
                type = data;
                path = space + 1;
            }
            else
            { // Header fields
                if (!domain && strncasecmp(data + i, "Host:", 5) == 0)
                {
                    domain = data + i + 5;
                    if (*domain == ' ')
                        domain++;
                }
                else if (!useragent && strncasecmp(data + i, "User-Agent:", 11) == 0)
                {
                    useragent = data + i + 11;
                    if (*useragent == ' ')
                        useragent++;
                }
                else if (!referer && strncasecmp(data + i, "Referer:", 8) == 0)
                {
                    referer = data + i + 8;
                    if (*referer == ' ')
                        referer++;
                }
                else if (!conttype && strncasecmp(data + i, "Content-Type:", 13) == 0)
                {
                    conttype = data + i + 13;
                    if (*conttype == ' ')
                        conttype++;
                }
                else if (!contlen && strncasecmp(data + i, "Content-Length:", 15) == 0)
                {
                    contlen = data + i + 15;
                    if (*contlen == ' ')
                        contlen++;
                }
            }

            i = j;
            while (i < len && (data[i] == 0 || data[i] == '\r' || data[j] == '\n'))
            {
                i++;
            }
        }

        if (!type)
            return;
    }
    else if (strncasecmp(data, "HTTP/1.", 7) == 0)
    { // HTTP Response

        uint32_t i = 0, j = 0;

        while (i < len - HTTP_FIELD_MINLEN && (!type || !conttype || !contlen || !referer || !useragent))
        {

            j = i;
            while (j < len - 1 && data[j] != '\r' && data[j] != '\n' && data[j] != 0)
            {
                if (data[j] < 32 || data[j] > 126)
                    data[j] = '_';
                j++;
            }
            data[j] = 0;

            if (i == 0)
            { // First line
                if (j < HTTP_STATUS_LINE_MINLEN)
                    return;
                if (data[8] != ' ')
                    return;
                if (j > 12)
                {
                    if (data[12] != ' ')
                        return;
                    data[12] = 0;
                }
                type = data + 9;
            }
            else
            { // Header fields
                if (!useragent && strncasecmp(data + i, "Server:", 7) == 0)
                {
                    useragent = data + i + 7;
                    if (*useragent == ' ')
                        useragent++;
                }
                else if (!referer && strncasecmp(data + i, "Location:", 9) == 0)
                {
                    referer = data + i + 9;
                    if (*referer == ' ')
                        referer++;
                }
                else if (!conttype && strncasecmp(data + i, "Content-Type:", 13) == 0)
                {
                    conttype = data + i + 13;
                    if (*conttype == ' ')
                        conttype++;
                }
                else if (!contlen && strncasecmp(data + i, "Content-Length:", 15) == 0)
                {
                    contlen = data + i + 15;
                    if (*contlen == ' ')
                        contlen++;
                }
            }

            i = j;
            while (i < len && (data[i] == 0 || data[i] == '\r' || data[j] == '\n'))
            {
                i++;
            }
        }

        if (!type)
            return;
    }
    else
    {
        return;
    }

    FILE *output = httpdump_file(rte_lcore_id());

    __print_ts(output, ts);
    fprintf(output, "|%s|%s|%s|%s|%s|%s|",
            type == NULL ? "" : type,
            domain == NULL ? "" : domain,
            path == NULL ? "" : path,
            conttype == NULL ? "" : conttype,
            contlen == NULL ? "" : contlen,
            referer == NULL ? "" : referer);

    __print_ip(output, src);
    fprintf(output, "|%u|", src->port);
    __print_ip(output, dst);
    fprintf(output, "|%u|%s\n", dst->port,
            useragent == NULL ? "" : useragent);
}
