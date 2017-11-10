#ifndef _HTTPDUMP_HTTP_
#define _HTTPDUMP_HTTP_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "httpdump_file.h"

#include <rte_lcore.h>

#define HTTP_HEADER_MINLEN 12
#define HTTP_FIELD_MINLEN 5
#define HTTP_REQUEST_LINE_MINLEN 14
#define HTTP_STATUS_LINE_MINLEN 12

inline void __print_ts(FILE *output, struct timeval ts)
{
    struct tm *ts_tm = localtime(&(ts.tv_sec));
    char str_time[20];
    strftime(str_time, sizeof str_time, "%Y-%m-%d %H:%M:%S", ts_tm);
    fprintf(output, "%s.%06ld", str_time, ts.tv_usec);
}

inline void __print_ip(FILE *output, host_t *host)
{
    if (host->ipver == 4)
    {
        struct in_addr addr;
        addr.s_addr = htobe32(host->ip4);
        // fprintf(output, "%s", inet_ntoa(addr));
    }
    else if (host->ipver == 6)
    {
        // fprintf(output, "\nipv6: \n");
        // fprintf(output, "%016llX%016llX ",
        // (unsigned long long)(host->ip6h),
        // (unsigned long long)(host->ip6l));

        char originaddr[41], formataddr[41];

        snprintf(originaddr, sizeof originaddr, "%016llX%016llX",
                 (unsigned long long)(host->ip6h),
                 (unsigned long long)(host->ip6l));
        snprintf(formataddr, sizeof formataddr, "%.4s:%.4s:%.4s:%.4s:%.4s:%.4s:%.4s:%.4s",
                 &originaddr[0], &originaddr[4], &originaddr[8], &originaddr[12],
                 &originaddr[16], &originaddr[20], &originaddr[24], &originaddr[28]);

        inet_pton(AF_INET6, formataddr, originaddr);
        inet_ntop(AF_INET6, originaddr, formataddr, INET6_ADDRSTRLEN);

        fprintf(output, "%s", formataddr);
        // fprintf(output, "%016llX%016llX", (unsigned long long)(host->ip6h), (unsigned long long)(host->ip6l));
    }
}

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

#endif //_HTTPDUMP_HTTP_
