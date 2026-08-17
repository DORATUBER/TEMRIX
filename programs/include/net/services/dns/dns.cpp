#include "dns.hpp"
#include "net/transport/udp.hpp"

namespace DNS
{
    static uint16_t g_txId    = 0x1234;
    static uint16_t g_srcPort = 54321;

    static int encodeName(const char *domain, uint8_t *out)
    {
        int pos = 0;
        int i   = 0;
        while (domain[i])
        {
            int labelStart = pos++;
            int len        = 0;
            while (domain[i] && domain[i] != '.')
            {
                out[pos++] = domain[i++];
                len++;
            }
            out[labelStart] = len;
            if (domain[i] == '.') i++;
        }
        out[pos++] = 0;
        return pos;
    }

    static uint16_t htons16(uint16_t v) { return (v >> 8) | (v << 8); }

    static const uint8_t *skipName(const uint8_t *ptr)
    {
        while (*ptr)
        {
            if ((*ptr & 0xC0) == 0xC0) { ptr += 2; return ptr; }
            ptr += *ptr + 1;
        }
        return ptr + 1;
    }

    int resolve(NetContext &ctx, const ResolveArgs &args)
    {
        uint8_t  buf[512];
        Memory::Set(buf, 0, 512);

        uint16_t txId    = g_txId++;
        uint16_t srcPort = g_srcPort++;

        Header *hdr  = reinterpret_cast<Header *>(buf);
        hdr->id      = htons16(txId);
        hdr->flags   = htons16(0x0100); 
        hdr->qdcount = htons16(1);

        int namelen = encodeName(args.domain, buf + 12);

        buf[12 + namelen + 0] = 0x00;
        buf[12 + namelen + 1] = 0x01;
        buf[12 + namelen + 2] = 0x00;
        buf[12 + namelen + 3] = 0x01;

        uint16_t queryLen = 12 + namelen + 4;

        UDP::SendArgs sendArgs{};
        sendArgs.destinationMac  = args.destinationMac;
        sendArgs.destinationIP   = args.dnsIP;
        sendArgs.sourceIP        = args.sourceIP;
        sendArgs.sourcePort      = srcPort;
        sendArgs.destinationPort = 53;
        sendArgs.payload         = buf;
        sendArgs.payloadLength   = queryLen;

        for (int retry = 0; retry < 3; retry++)
        {
            UDP::send(ctx, sendArgs);

            for (volatile int i = 0; i < 100000000; i++)
            {
                uint16_t pktLen = 0;
                if (ctx.nic.recv(args.packetBuffer, &pktLen) != 0) continue;

                UDP::Packet udp{};
                if (UDP::recv(args.packetBuffer, pktLen, &udp) != 0) continue;
                if (udp.destinationPort != srcPort)                  continue;

                uint16_t ancount = (udp.payload[6] << 8) | udp.payload[7];
                if (ancount == 0) return -1;

                const uint8_t *ptr = skipName(udp.payload + 12);
                ptr += 4;

                for (int a = 0; a < ancount; a++)
                {
                    ptr = skipName(ptr);

                    uint16_t type  = (ptr[0] << 8) | ptr[1];
                    ptr += 8;
                    uint16_t rdlen = (ptr[0] << 8) | ptr[1];
                    ptr += 2;

                    if (type == 1 && rdlen == 4)
                    {
                        args.ipOut[0] = ptr[0]; args.ipOut[1] = ptr[1];
                        args.ipOut[2] = ptr[2]; args.ipOut[3] = ptr[3];
                        return 0;
                    }

                    ptr += rdlen;
                }
            }
        }

        return -1;
    }
}