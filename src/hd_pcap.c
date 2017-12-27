#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pcap.h>

#include "hd_pcap.h"
#include "hd_pkt.h"

void httpdump_pcap(const struct pcap_pkthdr *pkthdr, const unsigned char *bytes)
{
    // Check minimum packet size
    // 14 (Ethernet) + 20 (IPv4) + 20 (TCP)
    if (pkthdr->len < 54)
        return;

    host_t src;
    host_t dst;
    char udp_flag = 0;
    memset(&src, 0, sizeof(host_t));
    memset(&dst, 0, sizeof(host_t));

    // Ethernet Header
    uint16_t i = 12;
    while ((bytes[i + 1] == 0x00 && (bytes[i] == 0x81 || bytes[i] == 0x91 || bytes[i] == 0x92 || bytes[i] == 0x93)) || (bytes[i] == 0x88 && bytes[i + 1] == 0xa8))
    {
        // VLAN tagging
        // EtherType = 0x8100, 0x9100, 0x9200, 0x9300, 0x88a8
        i += 4;
        if (i > pkthdr->len - 40)
            return;
    }

    // IP Header
    if (bytes[i] == 0x08 && bytes[i + 1] == 0x00)
    // if (0)
    {
        // IPv4
        i += 2;
        if ((bytes[i] & 0xF0) != 0x40) // Require: version = 4
            return;
        if (bytes[i + 9] != 6)      // Require: protocol = 6 (TCP)
            if (bytes[i + 9] != 17) // Require: protocol = 17 (UDP)
                return;
            else
                udp_flag = 1;
        src.ipver = 4;
        src.ip4 = be32toh(*(uint32_t *)&bytes[i + 12]);
        dst.ipver = 4;
        dst.ip4 = be32toh(*(uint32_t *)&bytes[i + 16]);
        i += (bytes[i] & 0x0F) << 2; // IHL
    }
    else if (bytes[i] == 0x86 && bytes[i + 1] == 0xDD)
    {
        // IPv6
        i += 2;
        if ((bytes[i] & 0xF0) != 0x60) // Require: version = 6
            return;
        /* TODO: IPv6 Extension Headers? */
        if (bytes[i + 9] != 6)      // Require: protocol = 6 (TCP)
            if (bytes[i + 9] != 17) // Require: protocol = 17 (UDP)
                return;
            else
                udp_flag = 1;
        src.ipver = 6;
        src.ip6h = be64toh(*(uint64_t *)&bytes[i + 8]);
        src.ip6l = be64toh(*(uint64_t *)&bytes[i + 16]);
        dst.ipver = 6;
        dst.ip6h = be64toh(*(uint64_t *)&bytes[i + 24]);
        dst.ip6l = be64toh(*(uint64_t *)&bytes[i + 32]);
        i += 40;
    }

    if (i > pkthdr->len - 20)
        return;

    if (udp_flag == 0)
    {
        // TCP Header
        // src.port = be16toh(*((uint16_t *)(bytes + i)));
        // dst.port = be16toh(*((uint16_t *)(bytes + i + 2)));
        // uint32_t seq = be32toh(*((uint32_t *)(bytes + i + 4)));
        // i += (bytes[i + 12] & 0xF0) >> 2;

        // if (i >= pkthdr->len)
        //     return;
        // httpdump_pkt((unsigned char *)(bytes + i), seq, pkthdr->len - i, pkthdr->ts, &src, &dst);
    }
    else
    {
        // UDP Header
        src.port = be16toh(*((uint16_t *)(bytes + i)));
        dst.port = be16toh(*((uint16_t *)(bytes + i + 2)));
        i += 8;

        if (i >= pkthdr->len)
            return;
        if (src.port == 53 || src.port == 53)
            httpdump_dns((unsigned char *)(bytes + i), pkthdr->len - i, pkthdr->ts, &src, &dst);
    }
}
