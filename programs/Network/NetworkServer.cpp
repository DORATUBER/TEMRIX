#include <temrixstd.h>
#include "e1000.hpp"
#include "net/link/net.hpp"
#include "net/link/ethernet.hpp"
#include "net/link/arp.hpp"
#include "net/internet/ip.hpp"
#include "net/transport/tcp.hpp"
#include "net/services/dns/dns.hpp"

static uint8_t g_packetBuffer[2048];
static uint8_t g_httpBuffer[8192];

struct ChunkedDecoder
{
    enum class State { ReadingSize, ReadingData, ReadingDataCRLF, ReadingTrailerCRLF, Done };

    State    state       = State::ReadingSize;
    uint32_t chunkSize   = 0;
    uint32_t chunkRemain = 0;

    static int hexVal(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    void feed(const uint8_t *data, uint16_t length)
    {
        for (uint16_t i = 0; i < length; i++)
        {
            uint8_t c = data[i];

            switch (state)
            {
            case State::ReadingSize:
            {
                int hv = hexVal((char)c);
                if (hv >= 0)
                {
                    chunkSize = chunkSize * 16 + (uint32_t)hv;
                }
                else if (c == '\n')
                {
                    chunkRemain = chunkSize;
                    chunkSize   = 0;

                    if (chunkRemain == 0)
                        state = State::Done;   
                    else
                        state = State::ReadingData;
                }
                break;
            }

            case State::ReadingData:
            {
                char buf[2] = { (char)c, '\0' };
                String::Print(buf);

                chunkRemain--;
                if (chunkRemain == 0)
                    state = State::ReadingDataCRLF;
                break;
            }

            case State::ReadingDataCRLF:
                if (c == '\n')
                    state = State::ReadingSize;
                break;

            case State::ReadingTrailerCRLF:
            case State::Done:
                break;
            }
        }
    }

    bool isDone() const { return state == State::Done; }
};

static int httpGet(NetContext &ctx, const char *host,
                    uint8_t *destinationMac, uint8_t *sourceIP, uint8_t *serverIP)
{
    TCP::Conn conn{};
    conn.ctx              = &ctx;
    conn.sourcePort        = 51000;
    conn.destinationPort   = 80;
    conn.packetBuffer       = g_packetBuffer;
    conn.state               = TCP::State::Closed;

    for (int i = 0; i < 6; i++) conn.destinationMac[i] = destinationMac[i];
    for (int i = 0; i < 4; i++) {
        conn.sourceIP[i]      = sourceIP[i];
        conn.destinationIP[i] = serverIP[i];
    }

    if (TCP::connect(conn) != 0)
    {
        String::Print("TCP connect failed.\n");
        return -1;
    }

    char request[512];
    String::snprintf(request, sizeof(request),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        host);

    uint16_t requestLen = 0;
    while (request[requestLen]) requestLen++;

    if (TCP::send(conn, request, requestLen) != 0)
    {
        String::Print("TCP send failed.\n");
        return -1;
    }

    bool           headersDone = false;
    int            crlfRun     = 0;  
    ChunkedDecoder decoder;

    while (true)
    {
        uint16_t length = 0;
        TCP::RecvArgs args{ g_httpBuffer, &length };

        int result = TCP::recv(conn, args);
        if (result < 0) break;

        if (length > 0)
        {
            uint16_t bodyStart = 0;

            if (!headersDone)
            {
                uint16_t i = 0;
                for (; i < length; i++)
                {
                    uint8_t c = g_httpBuffer[i];
                    if (c == '\r' && (crlfRun == 0 || crlfRun == 2)) crlfRun++;
                    else if (c == '\n' && (crlfRun == 1 || crlfRun == 3)) crlfRun++;
                    else crlfRun = 0;

                    if (crlfRun == 4)
                    {
                        headersDone = true;
                        bodyStart   = i + 1;
                        break;
                    }
                }

                if (!headersDone)
                    continue;   
            }

            if (bodyStart < length)
                decoder.feed(g_httpBuffer + bodyStart, length - bodyStart);
        }

        if (result == 1 || decoder.isDone())
            break;
    }

    String::Print("\n[response complete]\n");

    TCP::close(conn);
    return 0;
}

int main(int argc, char **argv)
{
    Syscall::Pci::KernelDevice devices[64];
    uint64_t count = Syscall::Pci::GetDevices(devices, 64);

    int nicIndex = -1;
    for (uint64_t i = 0; i < count; i++)
    {
        if (devices[i].vendorId == 0x8086 && devices[i].deviceId == 0x100E)
        {
            nicIndex = (int)i;
            break;
        }
    }

    if (nicIndex < 0)
    {
        String::Print("No E1000 found.\n");
        return -1;
    }

    uint64_t bar0 = Syscall::Memory::MapBar(nicIndex, 0);

    E1000::Controller nic;
    nic.init(bar0);

    NetContext ctx{ nic };

    uint8_t sourceIP[4]  = {192,168,100,2};
    uint8_t gatewayIP[4] = {192,168,100,1};
    uint8_t gatewayMac[6] = {};

    ARP::sendRequest(ctx, sourceIP, gatewayIP);
    ARP::sendRequest(ctx, sourceIP, gatewayIP);

    String::Print("Waiting for ARP reply...\n");

    while (true)
    {
        uint16_t length = 0;
        Ethernet::Frame *frame = Ethernet::recv(ctx, g_packetBuffer, &length);
        if (!frame) continue;
        if (frame->ethertype != ARP::EthertypeARP) continue;

        Memory::Copy(gatewayMac, frame->payload + 8, 6);
        break;
    }

    String::Printf("Gateway MAC: %x:%x:%x:%x:%x:%x\n",
        gatewayMac[0], gatewayMac[1], gatewayMac[2],
        gatewayMac[3], gatewayMac[4], gatewayMac[5]);

    uint8_t dnsServer[4] = {8,8,8,8};
    uint8_t resolvedIP[4];

    DNS::ResolveArgs dnsArgs{};
    dnsArgs.destinationMac = gatewayMac;
    dnsArgs.sourceIP       = sourceIP;
    dnsArgs.dnsIP          = dnsServer;
    dnsArgs.domain         = "httpforever.com";
    dnsArgs.ipOut          = resolvedIP;
    dnsArgs.packetBuffer   = g_packetBuffer;

    if (DNS::resolve(ctx, dnsArgs) != 0)
    {
        String::Print("DNS lookup failed.\n");
        return -1;
    }

    String::Printf("httpforever.com -> %d.%d.%d.%d\n",
        resolvedIP[0], resolvedIP[1], resolvedIP[2], resolvedIP[3]);

    httpGet(ctx, "httpforever.com", gatewayMac, sourceIP, resolvedIP);

    return 0;
}