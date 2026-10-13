#ifndef W6300LWIP_H
#define W6300LWIP_H

#include "arduino_mock.h"

class Wiznet6300lwIP {
public:
    Wiznet6300lwIP(int) {}
    bool begin() { return true; }
    bool connected() { return true; }
    IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
};

#endif
