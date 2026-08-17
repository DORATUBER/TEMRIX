#pragma once
#include <temrixstd.h>
#include "net.hpp"

namespace Ethernet
{
    constexpr uint16_t HeaderSize    = 14;
    constexpr uint16_t MinFrameSize  = 64;

    struct Frame
    {
        uint8_t  destinationMac[6];
        uint8_t  sourceMac[6];
        uint16_t ethertype;
        uint8_t  payload[];
    } __attribute__((packed));

    int    send(NetContext &ctx, uint8_t *destinationMac,
                uint16_t ethertype, void *payload, uint16_t payloadLength);

    Frame *recv(NetContext &ctx, uint8_t *buffer, uint16_t *totalLength);
}