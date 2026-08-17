#pragma once
#include <temrixstd.h>
#include "net/link/net.hpp"

namespace IP
{
    constexpr uint8_t ProtocolUDP = 17;
    constexpr uint8_t ProtocolTCP = 6;

    struct Header
    {
        uint8_t  versionAndHeaderLength;
        uint8_t  typeOfService;
        uint16_t totalLength;
        uint16_t identification;
        uint16_t flagsAndFragmentOffset;
        uint8_t  timeToLive;
        uint8_t  protocol;
        uint16_t checksum;
        uint8_t  sourceAddress[4];
        uint8_t  destinationAddress[4];
    } __attribute__((packed));

    struct Packet
    {
        uint8_t  sourceAddress[4];
        uint8_t  destinationAddress[4];
        uint8_t  protocol;
        uint8_t *payload;
        uint16_t payloadLength;
    };

    struct SendArgs
    {
        uint8_t *destinationMac;
        uint8_t *destinationIP;
        uint8_t *sourceIP;
        uint8_t  protocol;
        void    *payload;
        uint16_t payloadLength;
    };

    uint16_t computeChecksum(void *data, int length);
    void     send(NetContext &ctx, const SendArgs &args);
    int      recv(uint8_t *packetBuffer, uint16_t packetLength,
                  Packet *outPacket, bool verifyChecksum);
}