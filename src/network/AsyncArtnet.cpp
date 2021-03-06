//
// Created by Lukas Tenbrink on 02.02.20.
//

#include "AsyncArtnet.h"

#include <WiFi.h>

#include "ArtnetEndpoint.h"
#include "Network.h"
#include <util/Logger.h>

using namespace std::placeholders;

template <typename T>
bool AsyncArtnet<T>::listen(uint16_t port) {
    if (!udp.listen(port)) {
        SerialLog.print("UDP Listen failed!").ln();
        return false;
    }

    SerialLog.print("UDP Listening on Port: ").print(port).ln();
    udp.onPacket(std::bind(&AsyncArtnet::accept, this, _1));

    return true;
}

template <typename T>
bool AsyncArtnet<T>::accept(AsyncUDPPacket packet) {
    auto remoteIP = packet.remoteIP();

    uint8_t *packetData = packet.data();

    // Check that packetID is "Art-Net" else ignore
    if (memcmp(packetData, ART_NET_ID, 8) != 0)
        return 0;

    auto opcode = packetData[8] | packetData[9] << 8;

    if (opcode == ART_DMX) {
        if (packet.length() < 16)
            return 0;  // Packet too short

        channelPacket->sequence = packetData[12];
        channelPacket->remoteIP = remoteIP;

        channelPacket->data = packetData + ART_DMX_START;
        channelPacket->length = std::min<uint16_t>(
            packet.length() - ART_DMX_START,
            uint16_t(packetData[17]) | (uint16_t(packetData[16]) << 8)
        );

        auto incomingUniverse = packetData[14] | packetData[15] << 8;

        if (artDmxCallback) {
            for (auto channel : *channels) {
                if (channel->start > incomingUniverse || (channel->start + channel->length) <= incomingUniverse)
                    continue; // Out of range

                channelPacket->channel = channel;
                channelPacket->channelUniverse = incomingUniverse - channel->start;
                artDmxCallback(channelPacket);
            }
        }
        return ART_DMX;
    }
    if (opcode == ART_POLL) {
#if !defined(ARDUINO_SAMD_ZERO) && !defined(ESP8266) && !defined(ESP32)
        IPAddress local_ip = Ethernet.localIP();
#else
        IPAddress local_ip = Network::address();
#endif
        uint8_t node_ip_address[4];
        node_ip_address[0] = local_ip[0];
        node_ip_address[1] = local_ip[1];
        node_ip_address[2] = local_ip[2];
        node_ip_address[3] = local_ip[3];

        sprintf((char *) id, "Art-Net");
        memcpy(artPollReply.id, id, sizeof(artPollReply.id));
        memcpy(artPollReply.ip, node_ip_address, sizeof(artPollReply.ip));

        artPollReply.opCode = ART_POLL_REPLY;
        artPollReply.port = ART_NET_PORT;

        memset(artPollReply.goodinput, 0x08, 4);
        memset(artPollReply.goodoutput, 0x80, 4);
        memset(artPollReply.porttypes, 0xc0, 4);

        artPollReply.estaman[0] = 0;
        artPollReply.estaman[1] = 0;
        artPollReply.versionH = 1;
        artPollReply.versionL = 0;
        artPollReply.oemH = 0;
        artPollReply.oem = 0xFF;
        artPollReply.ubea = 0;
        artPollReply.status = 0xd2;
        artPollReply.swvideo = 0;
        artPollReply.swmacro = 0;
        artPollReply.swremote = 0;
        artPollReply.style = 0;

        artPollReply.numbportsH = 0;
        artPollReply.numbports = 4;
        artPollReply.status2 = 0x08;

        artPollReply.bindip[0] = node_ip_address[0];
        artPollReply.bindip[1] = node_ip_address[1];
        artPollReply.bindip[2] = node_ip_address[2];
        artPollReply.bindip[3] = node_ip_address[3];

        sprintf((char *) artPollReply.nodereport, "%i DMX output universes active.", artPollReply.numbports);

        for (const auto& channel : *channels) {
            if (!channel->isAdvertised)
                continue;

            uint8_t shortname[18];
            uint8_t longname[64];
            sprintf((char *) shortname, "Fan - %s", channel->name.c_str());
            sprintf((char *) longname, "Small LLED Fan - %s", channel->name.c_str());
            memcpy(artPollReply.shortname, shortname, sizeof(shortname));
            memcpy(artPollReply.longname, longname, sizeof(longname));

            for (int i = 0; i < (channel->length + 3) / 4; i++) {
                int packetStart = channel->start + i * 4;

                artPollReply.net = (packetStart >> 8) & 0x7f;
                artPollReply.subNet = (packetStart >> 4) & 0xf;

                uint8_t swin[4] = {};
                for (uint8_t j = 0; j < 4; j++) {
                    if (i * 4 + j < channel->length)
                        // Channel still supports packet_start + jth universe
                        swin[j] = static_cast<uint8_t>((packetStart + j) & 0xf);

                    artPollReply.swout[j] = swin[j];
                    artPollReply.swin[j] = swin[j];
                }

                udp.broadcastTo((uint8_t *)&artPollReply, sizeof(artPollReply), ART_NET_PORT);
//            udp.send(new AsyncUDPPacket(&udp, (uint8_t *)&ArtPollReply, ))
//            udp.beginPacket(broadcast, ART_NET_PORT);//send the packet to the broadcast address
//            Udp.write((uint8_t *)&ArtPollReply, sizeof(ArtPollReply));
//            Udp.endPacket();
            }
        }

        return ART_POLL;
    }
    if (opcode == ART_SYNC) {
        if (artSyncCallback)
            artSyncCallback(&remoteIP);
        return ART_SYNC;
    }

    SerialLog.print("Got unrecognized packet with opcode: " + String(opcode)).ln();

    return 0;
}

template <typename T>
bool AsyncArtnet<T>::print(AsyncUDPPacket packet) {
    Logger<SerialLogger> &log = SerialLog;
    log.print("UDP Packet Type: ");
    log.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
    log.print(", From: ");
    log.print(packet.remoteIP());
    log.print(":");
    log.print(packet.remotePort());
    log.print(", To: ");
    log.print(packet.localIP());
    log.print(":");
    log.print(packet.localPort());
    log.print(", Length: ");
    log.print(packet.length());
    log.print(", Data: ");
    log.print(packet.data());
    //reply to the client
//    packet.printf("Got %u bytes of data", packet.length());

    return false;
}

template <typename T>
int AsyncArtnet<T>::activePort() {
    return udp.connected() ? port : -1;
}

ArtnetChannel::ArtnetChannel(long start, long length, const String &name) : start(start), length(length), name(name) {}

// This is required here to build the template functions
// for all its uses.............. C++.
template class AsyncArtnet<ArtnetEndpoint>;
