#pragma once
#include <temrixstd.h>
#include "net/link/net.hpp"

namespace DNS
{
    struct Header
    {
        uint16_t id;
        uint16_t flags;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    } __attribute__((packed));

    struct ResolveArgs
    {
        uint8_t     *destinationMac;
        uint8_t     *sourceIP;
        uint8_t     *dnsIP;
        const char  *domain;
        uint8_t     *ipOut;
        uint8_t     *packetBuffer;
    };

    
    int resolve(NetContext &ctx, const ResolveArgs &args);
}