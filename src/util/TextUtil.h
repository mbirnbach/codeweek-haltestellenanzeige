#pragma once

#include <Arduino.h>

// Small UTF-8 aware text helpers. Stop and destination names contain
// umlauts ("Ihringshäuser Straße"), so cutting by bytes could split a
// character in half.

namespace text {

// Number of characters (code points), not bytes.
int utf8Length(const String& s);

// Shortens `s` to at most `maxChars` characters. If it had to be cut, the
// last character is replaced by `ellipsis` (e.g. "." -> "Ihringshäuser St.").
String utf8Truncate(const String& s, int maxChars, const char* ellipsis = ".");

// Pads with spaces (on the right, or on the left when alignRight) to exactly
// `width` characters, truncating first if needed.
String fitColumn(const String& s, int width, bool alignRight = false);

// Replaces German special characters with ASCII ("ä" -> "ae", "ß" -> "ss")
// and any other non-ASCII character with "?". For display fonts that only
// contain ASCII glyphs.
String toAscii(const String& s);

// Percent-encodes a query parameter value (UTF-8 safe).
String urlEncode(const String& s);

// Removes control characters so untrusted text can't mess up the terminal
// or the display.
String sanitize(const String& s);

}  // namespace text
