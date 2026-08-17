#include "udp.hpp"
#include "net/internet/ip.hpp"

namespace UDP
{
    constexpr uint16_t HeaderSize = 8;

    static uint16_t swap16(uint16_t v) { return (v >> 8) | ((v & 0xFF) << 8); }

    void send(NetContext &ctx, const SendArgs &args)
    {
        uint16_t udpLength = HeaderSize + args.payloadLength;
        if (udpLength > 1500)
            return;

        uint8_t buffer[1500];

        Header *header          = (Header *)buffer;
        header->sourcePort      = swap16(args.sourcePort);
        header->destinationPort = swap16(args.destinationPort);
        header->length          = swap16(udpLength);
        header->checksum        = 0;

        Memory::Copy(buffer + HeaderSize, args.payload, args.payloadLength);

        IP::SendArgs ipArgs;
        ipArgs.destinationMac = args.destinationMac;
        ipArgs.destinationIP  = args.destinationIP;
        ipArgs.sourceIP       = args.sourceIP;
        ipArgs.protocol       = IP::ProtocolUDP;
        ipArgs.payload        = buffer;
        ipArgs.payloadLength  = udpLength;

        IP::send(ctx, ipArgs);
    }

    int recv(uint8_t *packetBuffer, uint16_t packetLength, Packet *outPacket)
    {
        IP::Packet ipPacket;
        if (IP::recv(packetBuffer, packetLength, &ipPacket, false) != 0) return -1;
        if (ipPacket.protocol != IP::ProtocolUDP)                        return -1;
        if (ipPacket.payloadLength < HeaderSize)                         return -1;

        Header *header = (Header *)ipPacket.payload;

        outPacket->sourcePort      = swap16(header->sourcePort);
        outPacket->destinationPort = swap16(header->destinationPort);
        outPacket->payload         = ipPacket.payload + HeaderSize;
        outPacket->payloadLength   = ipPacket.payloadLength - HeaderSize;

        for (int i = 0; i < 4; i++) {
            outPacket->sourceAddress[i]      = ipPacket.sourceAddress[i];
            outPacket->destinationAddress[i] = ipPacket.destinationAddress[i];
        }

        return 0;
    }
}