#ifndef PUBSUBCLIENT_H
#define PUBSUBCLIENT_H

#include "arduino_mock.h"

extern std::string last_mqtt_status_sent;
extern int mqtt_status_call_count;

class PubSubClient {
public:
    PubSubClient(WiFiClientSecure&) {}
    void setServer(const char*, int) {}
    void setCallback(void (*)(char*, byte*, unsigned int)) {}
    void setBufferSize(int) {}
    bool connected() { return true; }
    void loop() {}
    bool connect(const char*, const char*, const char*) { return true; }
    bool subscribe(const char*) { return true; }
    int state() { return 0; }
    bool publish(const char*, const char* payload) {
        mqtt_status_call_count++;
        if (payload) {
            last_mqtt_status_sent = payload;
        }
        return true;
    }
};

#endif
