#include "arp.hpp"
#include "ethernet.hpp"

namespace ARP
{
    static uint16_t htons16(uint16_t v) { return (v >> 8) | (v << 8); }

    void sendRequest(NetContext &ctx, uint8_t *sourceIP, uint8_t *targetIP)
    {
        Packet packet;
        packet.hardwareType          = htons16(HardwareTypeEther);
        packet.protocolType          = htons16(ProtocolTypeIPv4);
        packet.hardwareAddressLength = 6;
        packet.protocolAddressLength = 4;
        packet.operation             = htons16(OperationRequest);

        for (int i = 0; i < 6; i++) {
            packet.senderMac[i] = ctx.nic.macAddress[i];
            packet.targetMac[i] = 0;
        }
        for (int i = 0; i < 4; i++) {
            packet.senderIP[i] = sourceIP[i];
            packet.targetIP[i] = targetIP[i];
        }

        uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        Ethernet::send(ctx, broadcast, EthertypeARP, &packet, sizeof(Packet));
    }
}