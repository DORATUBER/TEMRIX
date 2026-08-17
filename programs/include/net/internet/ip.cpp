#include "ip.hpp"
#include "net/link/ethernet.hpp"

namespace IP
{
    constexpr uint16_t EthertypeIPv4 = 0x0800;
    constexpr uint8_t  IPv4Version   = 4;
    constexpr uint8_t  DefaultTTL    = 64;
    constexpr uint8_t  HeaderDwords  = 5;
    constexpr uint16_t HeaderSize    = HeaderDwords * 4;

    uint16_t computeChecksum(void *data, int length)
    {
        uint16_t *ptr = (uint16_t *)data;
        uint32_t  sum = 0;

        while (length > 1) {
            sum    += *ptr++;
            length -= 2;
        }
        if (length)
            sum += *(uint8_t *)ptr;

        while (sum >> 16)
            sum = (sum & 0xFFFF) + (sum >> 16);

        return ~sum;
    }

    void send(NetContext &ctx, const SendArgs &args)
    {
        uint16_t totalLength = HeaderSize + args.payloadLength;
        if (totalLength > 1500)
            return; 

        uint8_t buffer[1500];

        Header *header = (Header *)buffer;
        header->versionAndHeaderLength = (IPv4Version << 4) | HeaderDwords;
        header->typeOfService          = 0;
        header->totalLength            = (totalLength >> 8) | ((totalLength & 0xFF) << 8);
        header->identification         = 0;
        header->flagsAndFragmentOffset = 0;
        header->timeToLive             = DefaultTTL;
        header->protocol               = args.protocol;
        header->checksum               = 0;

        for (int i = 0; i < 4; i++) {
            header->sourceAddress[i]      = args.sourceIP[i];
            header->destinationAddress[i] = args.destinationIP[i];
        }

        header->checksum = computeChecksum(header, HeaderSize);

        Memory::Copy(buffer + HeaderSize, args.payload, args.payloadLength);
        Ethernet::send(ctx, args.destinationMac, EthertypeIPv4, buffer, totalLength);
    }

    int recv(uint8_t *packetBuffer, uint16_t packetLength,
             Packet *outPacket, bool verifyChecksum)
    {
        uint16_t ethertype = (packetBuffer[12] << 8) | packetBuffer[13];
        if (ethertype != EthertypeIPv4)                          return -1;
        if (packetLength < Ethernet::HeaderSize + HeaderSize)    return -1;

        Header *header = (Header *)(packetBuffer + Ethernet::HeaderSize);
        if ((header->versionAndHeaderLength >> 4) != IPv4Version) return -1;

        uint8_t  headerLength  = (header->versionAndHeaderLength & 0x0F) * 4;
        uint16_t ipTotalLength = (header->totalLength >> 8)
                               | ((header->totalLength & 0xFF) << 8);

        if (verifyChecksum)
            if (computeChecksum(header, headerLength) != 0xFFFF)
                return -1;

        for (int i = 0; i < 4; i++) {
            outPacket->sourceAddress[i]      = header->sourceAddress[i];
            outPacket->destinationAddress[i] = header->destinationAddress[i];
        }

        outPacket->protocol      = header->protocol;
        outPacket->payload       = packetBuffer + Ethernet::HeaderSize + headerLength;
        outPacket->payloadLength = ipTotalLength - headerLength;

        return 0;
    }
}