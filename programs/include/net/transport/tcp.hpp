#pragma once
#include <temrixstd.h>
#include "net/link/net.hpp"
#include "net/internet/ip.hpp"

namespace TCP
{
    constexpr uint8_t FIN = 0x01;
    constexpr uint8_t SYN = 0x02;
    constexpr uint8_t RST = 0x04;
    constexpr uint8_t PSH = 0x08;
    constexpr uint8_t ACK = 0x10;

    constexpr uint8_t OptMSS = 0x01;

    enum class State : int
    {
        Closed      = 0,
        SynSent     = 1,
        Established = 2,
        FinWait     = 3,
    };

    struct Header
    {
        uint16_t sourcePort;
        uint16_t destinationPort;
        uint32_t seq;
        uint32_t ack;
        uint8_t  dataOffset;
        uint8_t  flags;
        uint16_t window;
        uint16_t checksum;
        uint16_t urgent;
    } __attribute__((packed));

    struct Conn
    {
        NetContext *ctx;
        uint8_t    destinationMac[6];
        uint8_t    sourceIP[4];
        uint8_t    destinationIP[4];
        uint16_t   sourcePort;
        uint16_t   destinationPort;
        uint32_t   seq;
        uint32_t   ack;
        State      state;
        uint8_t   *packetBuffer;
        uint16_t   lastPacketLength;
        int        waitTimer;
        uint8_t    waitFlags;
        bool       pendingFin;
    };

    struct RecvArgs
    {
        uint8_t  *buffer;
        uint16_t *length;
    };

    int  connect(Conn &conn);
    int  send(Conn &conn, void *data, uint16_t length);
    int  recv(Conn &conn, const RecvArgs &args);
    int  poll(Conn &conn, const RecvArgs &args);
    void close(Conn &conn);
    void ack(Conn &conn);
}