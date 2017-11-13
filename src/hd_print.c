/*
 * @Author: JesseMK
 * @Date: 2017-11-10 15:17:44
 * @Last Modified by: JesseMK
 * @Last Modified time: 2017-11-10 15:20:02
 */

/*
 * This is just a temp file for restructure.
 * Including two different print function.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

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
