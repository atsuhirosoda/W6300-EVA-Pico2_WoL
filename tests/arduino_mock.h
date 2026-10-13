#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

typedef unsigned char byte;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef bool boolean;

#define HEX 16
#define DEC 10

// Dummy IPAddress
class IPAddress {
public:
    IPAddress(int, int, int, int) {}
};

class String {
private:
    std::string str;
public:
    String() : str("") {}
    String(const char* s) : str(s ? s : "") {}
    explicit String(const std::string& s) : str(s) {}
    String(long val, int base = 10) {
        if (base == 16) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lX", val);
            str = buf;
        } else {
            str = std::to_string(val);
        }
    }

    String& operator+=(char c) {
        str += c;
        return *this;
    }

    String operator+(const String& other) const {
        return String(str + other.str);
    }

    String operator+(const char* other) const {
        return String(str + (other ? other : ""));
    }

    bool operator==(const char* other) const {
        return str == (other ? other : "");
    }

    bool operator==(const String& other) const {
        return str == other.str;
    }

    void trim() {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            str = "";
            return;
        }
        size_t end = str.find_last_not_of(" \t\r\n");
        str = str.substr(start, end - start + 1);
    }

    bool equalsIgnoreCase(const String& other) const {
        if (str.length() != other.str.length()) return false;
        for (size_t i = 0; i < str.length(); ++i) {
            if (std::tolower(static_cast<unsigned char>(str[i])) !=
                std::tolower(static_cast<unsigned char>(other.str[i]))) {
                return false;
            }
        }
        return true;
    }

    bool equalsIgnoreCase(const char* other) const {
        return equalsIgnoreCase(String(other));
    }

    const char* c_str() const {
        return str.c_str();
    }

    size_t length() const {
        return str.length();
    }
};

inline String operator+(const char* lhs, const String& rhs) {
    return String(lhs ? lhs : "") + rhs;
}

class FakeSerial {
public:
    void print(const char*) {}
    void print(const String&) {}
    void print(int) {}
    void print(int, int) {}
    void println(const char*) {}
    void println(const String&) {}
    void println(int) {}
    void println(int, int) {}
    void println(IPAddress) {}
    void println() {}
    void begin(int) {}
    void flush() {}
};

extern FakeSerial Serial;

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void delay(unsigned long) {}
inline unsigned long micros() { return 0; }
inline void randomSeed(uint32_t) {}
inline long random(long min, long) { return min; }
inline void tight_loop_contents() {}
inline void configTime(long, int, const char*, const char*) {}

// Mock time and state variables
extern unsigned long mock_millis_value;
inline unsigned long millis() {
    return mock_millis_value;
}

extern unsigned long lastWolCmdTime;

// Mock test tracking state
extern int magic_packet_call_count;
extern std::string last_mqtt_status_sent;
extern int mqtt_status_call_count;

#endif // ARDUINO_MOCK_H
