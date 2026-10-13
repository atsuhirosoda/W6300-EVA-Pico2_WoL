#ifndef WIFIUDP_H
#define WIFIUDP_H

#include "arduino_mock.h"

extern int magic_packet_call_count;

class WiFiUDP {
public:
    void begin(int) {}
    void beginPacket(IPAddress, int) { magic_packet_call_count++; }
    void write(const uint8_t*, size_t) {}
    void endPacket() {}
};

#endif
