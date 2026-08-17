#pragma once
#include <temrixstd.h>
#include "net.hpp"

namespace ARP
{
    constexpr uint16_t EthertypeARP      = 0x0806;
    constexpr uint16_t HardwareTypeEther = 0x0001;
    constexpr uint16_t ProtocolTypeIPv4  = 0x0800;
    constexpr uint16_t OperationRequest  = 0x0001;
    constexpr uint16_t OperationReply    = 0x0002;

    struct Packet
    {
        uint16_t hardwareType;
        uint16_t protocolType;
        uint8_t  hardwareAddressLength;
        uint8_t  protocolAddressLength;
        uint16_t operation;
        uint8_t  senderMac[6];
        uint8_t  senderIP[4];
        uint8_t  targetMac[6];
        uint8_t  targetIP[4];
    } __attribute__((packed));

    void sendRequest(NetContext &ctx, uint8_t *sourceIP, uint8_t *targetIP);
}