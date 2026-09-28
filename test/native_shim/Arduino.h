#pragma once

// Minimal host-side stand-in for <Arduino.h>, used ONLY by the "native" and
// "preview" PlatformIO environments. It provides the subset of Arduino's
// String class that the hardware-independent core and the student code use,
// so that code can be unit-tested on a normal computer.
//
// If a participant uses a String method that is missing here, the ESP32
// build still works; only the host tests would need this file extended.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

class String {
public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    String(char c) : s_(1, c) {}
    String(int v) : s_(std::to_string(v)) {}
    String(unsigned int v) : s_(std::to_string(v)) {}
    String(long v) : s_(std::to_string(v)) {}
    String(unsigned long v) : s_(std::to_string(v)) {}
    String(long long v) : s_(std::to_string(v)) {}

    const char* c_str() const { return s_.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(s_.size()); }
    bool isEmpty() const { return s_.empty(); }
    void reserve(unsigned int n) { s_.reserve(n); }
    char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
    char operator[](unsigned int i) const { return charAt(i); }

    String substring(unsigned int from) const { return substring(from, length()); }
    String substring(unsigned int from, unsigned int to) const {
        if (from > to) std::swap(from, to);
        if (from >= s_.size()) return String();
        to = std::min<unsigned int>(to, length());
        return String(s_.substr(from, to - from));
    }
    int indexOf(char c) const { return indexOf(String(c)); }
    int indexOf(const String& str) const {
        auto p = s_.find(str.s_);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    bool startsWith(const String& p) const { return s_.rfind(p.s_, 0) == 0; }
    bool endsWith(const String& p) const {
        return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
    }
    void toUpperCase() { for (auto& c : s_) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
    void toLowerCase() { for (auto& c : s_) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    void trim() {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s_.erase(s_.begin(), std::find_if(s_.begin(), s_.end(), notSpace));
        s_.erase(std::find_if(s_.rbegin(), s_.rend(), notSpace).base(), s_.end());
    }
    void replace(const String& from, const String& to) {
        if (from.s_.empty()) return;
        for (size_t p = 0; (p = s_.find(from.s_, p)) != std::string::npos; p += to.s_.size())
            s_.replace(p, from.s_.size(), to.s_);
    }
    long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }

    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o) { s_ += (o ? o : ""); return *this; }
    String& operator+=(char c) { s_ += c; return *this; }
    String& operator+=(int v) { s_ += std::to_string(v); return *this; }

    friend String operator+(String a, const String& b) { a += b; return a; }
    friend String operator+(String a, const char* b) { a += b; return a; }
    friend String operator+(const char* a, const String& b) { return String(a) + b; }
    friend String operator+(String a, char b) { a += b; return a; }
    friend String operator+(String a, int b) { a += b; return a; }

    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    bool operator==(const char* o) const { return s_ == (o ? o : ""); }
    bool operator!=(const char* o) const { return !(*this == o); }

private:
    std::string s_;
};
