#pragma once

// Host stand-in for Arduino's Print class (and the few other Arduino bits
// Adafruit_GFX.cpp needs), used only by tools/render_cyd.sh.

#include <Arduino.h>  // test/native_shim: String

#include <cstddef>
#include <cstdint>

class __FlashStringHelper;

inline void yield() {}
inline float radians(float deg) { return deg * 0.017453292519943295f; }

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buf, size_t n) {
        size_t written = 0;
        while (n--) written += write(*buf++);
        return written;
    }
    size_t print(const String& s) { return write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length()); }
    size_t print(const char* s) { return print(String(s)); }
};
