#include <iostream>
#include <cassert>
#include <cstring>
#include "arduino_mock.h"

// Forward declaration of mqttCallback function from mqtt_callback_impl.cpp
void mqttCallback(char* topic, byte* payload, unsigned int length);

// Helper function to reset test state between test cases
void resetTestState() {
    magic_packet_call_count = 0;
    mqtt_status_call_count = 0;
    last_mqtt_status_sent = "";
    lastWolCmdTime = 0;
    mock_millis_value = 100000;
}

// Helper to check if published status ends with expected string (since getTimestampString prepends timestamp)
bool statusEndsWith(const std::string& fullStatus, const std::string& suffix) {
    if (fullStatus.length() < suffix.length()) return false;
    return fullStatus.compare(fullStatus.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Test cases
void test_valid_commands_trigger_wol() {
    std::cout << "[RUNNING] test_valid_commands_trigger_wol..." << std::endl;
    char topic[] = "user/feeds/cmd";

    // Test 1: "WOL"
    resetTestState();
    const char payload1[] = "WOL";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload1)), strlen(payload1));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));

    // Test 2: "wol" (lowercase / case-insensitivity check)
    resetTestState();
    const char payload2[] = "wol";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload2)), strlen(payload2));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));

    // Test 3: "ON"
    resetTestState();
    const char payload3[] = "ON";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload3)), strlen(payload3));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));

    // Test 4: "1"
    resetTestState();
    const char payload4[] = "1";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload4)), strlen(payload4));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));

    // Test 5: " WOL \r\n " (with leading/trailing whitespace)
    resetTestState();
    const char payload5[] = " WOL \r\n ";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload5)), strlen(payload5));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));

    std::cout << "[PASSED] test_valid_commands_trigger_wol" << std::endl;
}

void test_rate_limiting_cooldown() {
    std::cout << "[RUNNING] test_rate_limiting_cooldown..." << std::endl;
    char topic[] = "user/feeds/cmd";

    resetTestState();
    mock_millis_value = 50000;

    // First valid command -> succeeds
    const char payload[] = "WOL";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload)), strlen(payload));
    assert(magic_packet_call_count == 1);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));
    assert(lastWolCmdTime == 50000);

    // Second command at 5000ms later (< 10000ms cooldown) -> rate limited
    mock_millis_value = 55000;
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload)), strlen(payload));
    assert(magic_packet_call_count == 1); // Not incremented
    assert(statusEndsWith(last_mqtt_status_sent, "RATE_LIMIT: Command ignored (cooldown active)"));

    // Third command exactly at cooldown threshold (60000ms - 50000ms = 10000ms) -> succeeds
    mock_millis_value = 60000;
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload)), strlen(payload));
    assert(magic_packet_call_count == 2);
    assert(statusEndsWith(last_mqtt_status_sent, "ACK: WoL packet successfully sent"));
    assert(lastWolCmdTime == 60000);

    std::cout << "[PASSED] test_rate_limiting_cooldown" << std::endl;
}

void test_unknown_commands_rejection() {
    std::cout << "[RUNNING] test_unknown_commands_rejection..." << std::endl;
    char topic[] = "user/feeds/cmd";

    // Test 1: "RUN"
    resetTestState();
    const char payload1[] = "RUN";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload1)), strlen(payload1));
    assert(magic_packet_call_count == 0);
    assert(statusEndsWith(last_mqtt_status_sent, "REJECT: Unknown command 'RUN'"));

    // Test 2: "OFF"
    resetTestState();
    const char payload2[] = "OFF";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload2)), strlen(payload2));
    assert(magic_packet_call_count == 0);
    assert(statusEndsWith(last_mqtt_status_sent, "REJECT: Unknown command 'OFF'"));

    // Test 3: "0"
    resetTestState();
    const char payload3[] = "0";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload3)), strlen(payload3));
    assert(magic_packet_call_count == 0);
    assert(statusEndsWith(last_mqtt_status_sent, "REJECT: Unknown command '0'"));

    // Test 4: Empty string
    resetTestState();
    const char payload4[] = "";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload4)), 0);
    assert(magic_packet_call_count == 0);
    assert(statusEndsWith(last_mqtt_status_sent, "REJECT: Unknown command ''"));

    // Test 5: Whitespace only
    resetTestState();
    const char payload5[] = "   \t \n ";
    mqttCallback(topic, const_cast<byte*>(reinterpret_cast<const byte*>(payload5)), strlen(payload5));
    assert(magic_packet_call_count == 0);
    assert(statusEndsWith(last_mqtt_status_sent, "REJECT: Unknown command ''"));

    std::cout << "[PASSED] test_unknown_commands_rejection" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " Running Unit Tests for mqttCallback   " << std::endl;
    std::cout << "========================================" << std::endl;

    test_valid_commands_trigger_wol();
    test_rate_limiting_cooldown();
    test_unknown_commands_rejection();

    std::cout << "========================================" << std::endl;
    std::cout << " All tests passed successfully!         " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
