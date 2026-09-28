#include "util/TextUtil.h"

namespace text {

namespace {

bool isContinuationByte(char c) { return (static_cast<uint8_t>(c) & 0xC0) == 0x80; }

// Byte offset where character number `charIndex` starts.
unsigned int byteOffsetOfChar(const String& s, int charIndex) {
    int chars = 0;
    for (unsigned int i = 0; i < s.length(); ++i) {
        if (!isContinuationByte(s[i])) {
            if (chars == charIndex) return i;
            ++chars;
        }
    }
    return s.length();
}

}  // namespace

int utf8Length(const String& s) {
    int n = 0;
    for (unsigned int i = 0; i < s.length(); ++i) {
        if (!isContinuationByte(s[i])) ++n;
    }
    return n;
}

String utf8Truncate(const String& s, int maxChars, const char* ellipsis) {
    if (maxChars <= 0) return String();
    if (utf8Length(s) <= maxChars) return s;

    int ellipsisChars = ellipsis ? utf8Length(String(ellipsis)) : 0;
    if (ellipsisChars >= maxChars) ellipsisChars = 0;

    String out = s.substring(0, byteOffsetOfChar(s, maxChars - ellipsisChars));
    // Don't leave "Kassel ." – drop trailing spaces before the ellipsis.
    while (out.length() > 0 && out[out.length() - 1] == ' ') {
        out = out.substring(0, out.length() - 1);
    }
    if (ellipsisChars > 0) out += ellipsis;
    return out;
}

String fitColumn(const String& s, int width, bool alignRight) {
    String cut = utf8Truncate(s, width);
    String pad;
    for (int i = utf8Length(cut); i < width; ++i) pad += ' ';
    return alignRight ? pad + cut : cut + pad;
}

String toAscii(const String& s) {
    String out;
    out.reserve(s.length());
    for (unsigned int i = 0; i < s.length(); ++i) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            continue;
        }
        if (c == 0xC3 && i + 1 < s.length()) {
            switch (static_cast<uint8_t>(s[i + 1])) {
                case 0xA4: out += "ae"; ++i; continue;  // ä
                case 0xB6: out += "oe"; ++i; continue;  // ö
                case 0xBC: out += "ue"; ++i; continue;  // ü
                case 0x84: out += "Ae"; ++i; continue;  // Ä
                case 0x96: out += "Oe"; ++i; continue;  // Ö
                case 0x9C: out += "Ue"; ++i; continue;  // Ü
                case 0x9F: out += "ss"; ++i; continue;  // ß
                case 0xA9: out += "e"; ++i; continue;   // é
                case 0xA8: out += "e"; ++i; continue;   // è
                default: break;
            }
        }
        if (c == 0xC2 && i + 1 < s.length() && static_cast<uint8_t>(s[i + 1]) == 0xB7) {  // ·
            out += '-';
            ++i;
            continue;
        }
        // Any other multi-byte character: skip its continuation bytes.
        if (!isContinuationByte(static_cast<char>(c))) out += '?';
    }
    return out;
}

String urlEncode(const String& s) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);
    for (unsigned int i = 0; i < s.length(); ++i) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

String sanitize(const String& s) {
    String out;
    out.reserve(s.length());
    for (unsigned int i = 0; i < s.length(); ++i) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x20 || c == 0x7F) {
            out += ' ';
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

}  // namespace text
