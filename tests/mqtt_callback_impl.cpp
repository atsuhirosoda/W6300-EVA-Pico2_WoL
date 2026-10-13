#include "arduino_mock.h"
#include "hardware/structs/rosc.h"

FakeSerial Serial;
rosc_hw_t rosc_hw_obj = { 0 };
unsigned long mock_millis_value = 100000;

int magic_packet_call_count = 0;
std::string last_mqtt_status_sent = "";
int mqtt_status_call_count = 0;

// Directly include the source file to compile and test the actual source
#include "W6300-EVB-Pico2-WoL - コピー.ino"
