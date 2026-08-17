#include "ethernet.hpp"

namespace Ethernet
{
    int send(NetContext &ctx, uint8_t *destinationMac,
            uint16_t ethertype, void *payload, uint16_t payloadLength)
    {
        uint16_t totalSize = HeaderSize + payloadLength;
        if (totalSize < MinFrameSize)
            totalSize = MinFrameSize;
        if (totalSize > 1518)
            return -1; 

        uint8_t buffer[1518];
        Memory::Set(buffer, 0, totalSize);

        buffer[0] = destinationMac[0]; buffer[1] = destinationMac[1];
        buffer[2] = destinationMac[2]; buffer[3] = destinationMac[3];
        buffer[4] = destinationMac[4]; buffer[5] = destinationMac[5];

        buffer[6]  = ctx.nic.macAddress[0]; buffer[7]  = ctx.nic.macAddress[1];
        buffer[8]  = ctx.nic.macAddress[2]; buffer[9]  = ctx.nic.macAddress[3];
        buffer[10] = ctx.nic.macAddress[4]; buffer[11] = ctx.nic.macAddress[5];

        buffer[12] = (ethertype >> 8) & 0xFF;
        buffer[13] =  ethertype       & 0xFF;

        Memory::Copy(buffer + HeaderSize, payload, payloadLength);

        return ctx.nic.send(buffer, totalSize);
    }

    Frame *recv(NetContext &ctx, uint8_t *buffer, uint16_t *totalLength)
    {
        uint16_t length = 0;
        if (ctx.nic.recv(buffer, &length) != 0)
            return nullptr;

        Frame *frame     = (Frame *)buffer;
        frame->ethertype = (buffer[12] << 8) | buffer[13];

        if (totalLength)
            *totalLength = length;

        return frame;
    }
}