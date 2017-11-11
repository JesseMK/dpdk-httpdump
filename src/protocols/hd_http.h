#ifndef _HTTPDUMP_HTTP_
#define _HTTPDUMP_HTTP_

#include "../hd_pkt.h"

#define HTTP_HEADER_MINLEN 12
#define HTTP_FIELD_MINLEN 5
#define HTTP_REQUEST_LINE_MINLEN 14
#define HTTP_STATUS_LINE_MINLEN 12

void httpdump_http(unsigned char *data, uint32_t len, struct timeval ts, host_t *src, host_t *dst);

#endif //_HTTPDUMP_HTTP_
