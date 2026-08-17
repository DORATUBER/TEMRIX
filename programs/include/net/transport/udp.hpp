#pragma once
#include <temrixstd.h>
#include "net/link/net.hpp"
#include "net/internet/ip.hpp"

namespace UDP
{
    struct Header
    {
        uint16_t sourcePort;
        uint16_t destinationPort;
        uint16_t length;
        uint16_t checksum;
    } __attribute__((packed));

    struct Packet
    {
        uint8_t  sourceAddress[4];
        uint8_t  destinationAddress[4];
        uint16_t sourcePort;
        uint16_t destinationPort;
        uint8_t *payload;
        uint16_t payloadLength;
    };

    struct SendArgs
    {
        uint8_t *destinationMac;
        uint8_t *destinationIP;
        uint8_t *sourceIP;
        uint16_t sourcePort;
        uint16_t destinationPort;
        void    *payload;
        uint16_t payloadLength;
    };

    void send(NetContext &ctx, const SendArgs &args);
    int  recv(uint8_t *packetBuffer, uint16_t packetLength, Packet *outPacket);
}