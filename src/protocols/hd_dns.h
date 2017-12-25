#ifndef _HTTPDUMP_DNS_
#define _HTTPDUMP_DNS_

#include "../hd_pkt.h"

#define DNS_HEADER_LEN 12
#define DNS_FIELD_MINLEN 5

void httpdump_dns(unsigned char *data, uint32_t len, struct timeval ts, host_t *src, host_t *dst);

#endif //_HTTPDUMP_DNS_
