#include "tcp.hpp"
#include "net/internet/ip.hpp"
#include "net/link/ethernet.hpp"

namespace TCP
{
    static uint16_t checksum(uint8_t *srcIP, uint8_t *dstIP,
                             void *tcpSeg, uint16_t tcpLen)
    {
        uint8_t pseudo[12];
        pseudo[0] = srcIP[0]; pseudo[1] = srcIP[1];
        pseudo[2] = srcIP[2]; pseudo[3] = srcIP[3];
        pseudo[4] = dstIP[0]; pseudo[5] = dstIP[1];
        pseudo[6] = dstIP[2]; pseudo[7] = dstIP[3];
        pseudo[8]  = 0;
        pseudo[9]  = 6;
        pseudo[10] = (tcpLen >> 8) & 0xFF;
        pseudo[11] =  tcpLen       & 0xFF;

        uint32_t  sum = 0;
        uint16_t *ptr = (uint16_t *)pseudo;
        for (int i = 0; i < 6; i++) sum += ptr[i];

        ptr = (uint16_t *)tcpSeg;
        int len = tcpLen;
        while (len > 1) { sum += *ptr++; len -= 2; }
        if (len) sum += *(uint8_t *)ptr;

        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        return ~sum;
    }

    static uint32_t swap32(uint32_t v)
    {
        return ((v & 0xFF) << 24) | (((v >> 8) & 0xFF) << 16) |
               (((v >> 16) & 0xFF) << 8) | ((v >> 24) & 0xFF);
    }

    static uint16_t swap16(uint16_t v)
    {
        return (v >> 8) | ((v & 0xFF) << 8);
    }

    static void sendRaw(Conn &conn, uint8_t flags, uint8_t options,
                        void *data, uint16_t dataLen)
    {
        bool     hasMSS  = options & OptMSS;
        uint8_t  synOpts = hasMSS ? 4 : 0;
        uint16_t tcpLen  = 20 + synOpts + dataLen;
        if (tcpLen > 1460)
            return; 

        uint8_t buf[1460];
        Memory::Set(buf, 0, tcpLen);

        Header *tcp          = (Header *)buf;
        tcp->sourcePort      = swap16(conn.sourcePort);
        tcp->destinationPort = swap16(conn.destinationPort);
        tcp->seq             = swap32(conn.seq);
        tcp->ack             = swap32(conn.ack);
        tcp->dataOffset      = hasMSS ? 0x60 : 0x50;
        tcp->flags           = flags;
        tcp->window          = 0x0045;
        tcp->checksum        = 0;

        if (hasMSS) {
            buf[20] = 0x02;
            buf[21] = 0x04;
            buf[22] = 0x05;
            buf[23] = 0x78;
        }

        if (data && dataLen)
            Memory::Copy(buf + 20 + synOpts, data, dataLen);

        tcp->checksum = checksum(conn.sourceIP, conn.destinationIP, buf, tcpLen);

        IP::SendArgs args;
        args.destinationMac = conn.destinationMac;
        args.destinationIP  = conn.destinationIP;
        args.sourceIP       = conn.sourceIP;
        args.protocol       = IP::ProtocolTCP;
        args.payload        = buf;
        args.payloadLength  = tcpLen;

        IP::send(*conn.ctx, args);
    }

    int poll(Conn &conn, const RecvArgs &args)
    {
        if (conn.waitTimer == 0) return -1;
        if (conn.waitTimer > 0) conn.waitTimer--;

        if (conn.pendingFin) {
            conn.pendingFin = false;
            if (args.length) *args.length = 0;
            return 2;
        }

        uint16_t pktLen = 0;
        if (conn.ctx->nic.recv(conn.packetBuffer, &pktLen) != 0) {
            if (conn.waitTimer == 0) return -1;
            conn.waitTimer--;
            return 0;
        }

        IP::Packet ip;
        if (IP::recv(conn.packetBuffer, pktLen, &ip, false) != 0) return 0;
        if (ip.protocol != IP::ProtocolTCP)                        return 0;
        if (ip.payloadLength < 20)                                 return 0;

        Header  *tcp     = (Header *)ip.payload;
        uint16_t dstPort = swap16(tcp->destinationPort);
        uint16_t srcPort = swap16(tcp->sourcePort);

        if (dstPort != conn.sourcePort)                        return 0;
        if (srcPort != conn.destinationPort)                   return 0;
        if ((tcp->flags & conn.waitFlags) != conn.waitFlags)  return 0;

        conn.waitTimer = 200000000;

        uint32_t remoteSeq = swap32(tcp->seq);
        uint8_t  hdrLen    = (tcp->dataOffset >> 4) * 4;
        uint16_t dataLen   = ip.payloadLength - hdrLen;

        conn.ack           = remoteSeq + (dataLen > 0 ? dataLen : 1);
        conn.lastPacketLength = pktLen;

        if (args.buffer && args.length && dataLen > 0) {
            Memory::Copy(args.buffer, ip.payload + hdrLen, dataLen);
            *args.length = dataLen;
            if (tcp->flags & FIN)
                conn.pendingFin = true;
            return 1;
        }

        if (args.length) *args.length = 0;
        if (tcp->flags & FIN) return 2;
        return 1;
    }

    static int wait(Conn &conn, uint8_t wantFlags, const RecvArgs &args)
    {
        conn.waitFlags = wantFlags;
        conn.waitTimer = 200000000;
        int r;
        while ((r = poll(conn, args)) == 0);
        if (r == -1) return -1;
        return (r == 2) ? 1 : 0;
    }

    int connect(Conn &conn)
    {
        uint32_t isn = 0x45678901 + 0x123457;
        conn.seq   = isn ^ ((uint32_t)conn.destinationIP[3] << 24) ^
                     ((uint32_t)conn.sourcePort << 16);
        conn.ack   = 0;
        conn.state = State::SynSent;

        sendRaw(conn, SYN, OptMSS, nullptr, 0);
        conn.seq++;

        RecvArgs none { nullptr, nullptr };
        if (wait(conn, SYN | ACK, none) != 0) return -1;

        conn.state = State::Established;
        sendRaw(conn, ACK, 0, nullptr, 0);
        return 0;
    }

    int send(Conn &conn, void *data, uint16_t length)
    {
        if (conn.state != State::Established) return -1;
        sendRaw(conn, ACK | PSH, 0, data, length);
        conn.seq += length;
        return 0;
    }

    int recv(Conn &conn, const RecvArgs &args)
    {
        if (conn.state != State::Established) return -1;
        if (args.length) *args.length = 0;

        int result = wait(conn, ACK, args);
        if (result < 0) return -1;

        sendRaw(conn, ACK, 0, nullptr, 0);

        if (result == 1) {
            conn.state = State::FinWait;
            return 1;
        }
        return 0;
    }

    void close(Conn &conn)
    {
        if (conn.state == State::Closed) return;
        sendRaw(conn, FIN | ACK, 0, nullptr, 0);
        conn.seq++;
        conn.state = State::FinWait;

        RecvArgs none { nullptr, nullptr };
        wait(conn, ACK, none);
        sendRaw(conn, ACK, 0, nullptr, 0);
        conn.state = State::Closed;
    }

    void ack(Conn &conn)
    {
        sendRaw(conn, ACK, 0, nullptr, 0);
    }
}