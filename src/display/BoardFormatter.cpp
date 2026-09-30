#include "display/BoardFormatter.h"

#include "student/config_checks.h"
#include "util/TextUtil.h"

namespace board_text {

namespace {

const char* ansiForeground(DisplayColor c) {
    switch (c) {
        case COLOR_BLACK:  return "\033[30m";
        case COLOR_WHITE:  return "\033[97m";
        case COLOR_GRAY:   return "\033[90m";
        case COLOR_RED:    return "\033[91m";
        case COLOR_ORANGE: return "\033[38;5;208m";
        case COLOR_YELLOW: return "\033[93m";
        case COLOR_GREEN:  return "\033[92m";
        case COLOR_CYAN:   return "\033[96m";
        case COLOR_BLUE:   return "\033[94m";
        case COLOR_PURPLE: return "\033[95m";
    }
    return "";
}

const char* ansiBackground(DisplayColor c) {
    switch (c) {
        case COLOR_BLACK:  return "\033[40m";
        case COLOR_WHITE:  return "\033[107m";
        case COLOR_GRAY:   return "\033[100m";
        case COLOR_RED:    return "\033[101m";
        case COLOR_ORANGE: return "\033[48;5;208m";
        case COLOR_YELLOW: return "\033[103m";
        case COLOR_GREEN:  return "\033[102m";
        case COLOR_CYAN:   return "\033[106m";
        case COLOR_BLUE:   return "\033[104m";
        case COLOR_PURPLE: return "\033[105m";
    }
    return "";
}

constexpr const char* ANSI_RESET = "\033[0m";

// Builds one output line out of coloured segments.
class Line {
public:
    explicit Line(bool ansi) : ansi_(ansi) {
        if (ansi_) text_ += ansiBackground(BACKGROUND_COLOR);
    }
    Line& add(const String& s, DisplayColor color) {
        if (ansi_) text_ += ansiForeground(color);
        text_ += s;
        return *this;
    }
    String done() {
        if (ansi_) text_ += ANSI_RESET;
        return text_;
    }

private:
    bool ansi_;
    String text_;
};

String repeat(char c, int n) {
    String s;
    for (int i = 0; i < n; ++i) s += c;
    return s;
}

String separator(bool ansi) { return Line(ansi).add(repeat('-', WIDTH), TEXT_COLOR).done(); }

String centered(const String& s, int width) {
    String cut = text::utf8Truncate(s, width);
    int left = (width - text::utf8Length(cut)) / 2;
    return text::fitColumn(repeat(' ', left) + cut, width);
}

// Red for warnings, unless the background is red already.
DisplayColor alertColor() { return BACKGROUND_COLOR == COLOR_RED ? COLOR_WHITE : COLOR_RED; }

DisplayColor statusColor(BoardStatus s) {
    switch (s) {
        case BoardStatus::Live: return BACKGROUND_COLOR == COLOR_GREEN ? COLOR_WHITE : COLOR_GREEN;
        case BoardStatus::Stale: return BACKGROUND_COLOR == COLOR_ORANGE ? COLOR_WHITE : COLOR_ORANGE;
        case BoardStatus::Offline: return alertColor();
    }
    return TEXT_COLOR;
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

std::vector<String> departures(const DisplayModel& m, bool ansi) {
    std::vector<String> out;

    // Header: stop name left, clock right.
    const int clockWidth = m.clock.length() > 0 ? text::utf8Length(m.clock) + 1 : 0;
    out.push_back(separator(ansi));
    out.push_back(Line(ansi)
                      .add(text::fitColumn(m.stopName, WIDTH - clockWidth), HEADER_COLOR)
                      .add(clockWidth > 0 ? String(" ") + m.clock : String(), TEXT_COLOR)
                      .done());
    out.push_back(separator(ansi));

    // Column widths depend on the actual content, within fixed limits, so
    // no column can ever push into its neighbour.
    int lineWidth = 2, platformWidth = 1, timeWidth = 5;
    for (int i = 0; i < m.rowCount; ++i) {
        lineWidth = clampInt(text::utf8Length(m.rows[i].line), lineWidth, 4);
        platformWidth = clampInt(text::utf8Length(m.rows[i].platform), platformWidth, 2);
        timeWidth = clampInt(text::utf8Length(m.rows[i].time), timeWidth, MAX_TIME_CHARS);
    }
    const int fixed = lineWidth + 1 + 1 + (m.showPlatform ? platformWidth + 1 : 0) + timeWidth;
    const int destWidth = WIDTH - fixed;

    for (int i = 0; i < m.rowCount; ++i) {
        const DepartureRow& r = m.rows[i];
        Line line(ansi);
        line.add(text::fitColumn(r.line, lineWidth) + " ", LINE_COLOR);
        line.add(text::fitColumn(r.destination, destWidth) + " ", TEXT_COLOR);
        if (m.showPlatform) line.add(text::fitColumn(r.platform, platformWidth) + " ", TEXT_COLOR);
        line.add(text::fitColumn(r.time, timeWidth, true), r.cancelled ? alertColor() : TIME_COLOR);
        out.push_back(line.done());
    }
    if (m.rowCount == 0) {
        out.push_back(Line(ansi).add(centered("Keine Abfahrten in nächster Zeit", WIDTH), TEXT_COLOR).done());
    }
    // Scrolls on the CYD; here the start of it (full text in the Serial log above).
    if (m.ticker.length() > 0) {
        out.push_back(Line(ansi).add(text::fitColumn(String("» ") + m.ticker, WIDTH), TEXT_COLOR).done());
    }

    out.push_back(separator(ansi));
    // Status bar only when something is off (stale / offline).
    if (m.statusText.length() > 0) {
        out.push_back(Line(ansi).add(text::fitColumn(m.statusText, WIDTH), statusColor(m.status)).done());
        out.push_back(separator(ansi));
    }
    return out;
}

std::vector<String> status(const StatusScreen& s, bool ansi) {
    std::vector<String> out;
    out.push_back(separator(ansi));
    out.push_back(Line(ansi).add(centered(s.title, WIDTH), s.isError ? alertColor() : HEADER_COLOR).done());
    if (s.detail.length() > 0) out.push_back(Line(ansi).add(centered(s.detail, WIDTH), TEXT_COLOR).done());
    if (s.hint.length() > 0) out.push_back(Line(ansi).add(centered(s.hint, WIDTH), TIME_COLOR).done());
    out.push_back(separator(ansi));
    return out;
}

std::vector<String> setup(const SetupScreen& s, bool ansi) {
    std::vector<String> out;
    out.push_back(separator(ansi));
    out.push_back(Line(ansi).add(centered("EINRICHTUNG", WIDTH), HEADER_COLOR).done());
    out.push_back(separator(ansi));
    out.push_back(Line(ansi).add(text::fitColumn("1. Mit dem Handy ins WLAN:", WIDTH), TEXT_COLOR).done());
    out.push_back(Line(ansi).add(text::fitColumn(String("   ") + s.apName, WIDTH), TIME_COLOR).done());
    out.push_back(Line(ansi).add(text::fitColumn("2. Im Browser öffnen:", WIDTH), TEXT_COLOR).done());
    out.push_back(Line(ansi).add(text::fitColumn(String("   ") + s.url, WIDTH), TIME_COLOR).done());
    out.push_back(separator(ansi));
    out.push_back(Line(ansi).add(text::fitColumn(s.step, WIDTH), LINE_COLOR).done());
    out.push_back(separator(ansi));
    return out;
}

String designSummary() {
    String s = "Design: ";
    s += DEPARTURE_COUNT;
    s += " Abfahrten, Schrift ";
    s += displayFontName(FONT);
    s += ", 0 min = \"";
    s += NOW_TEXT;
    s += "\"";
    s += SHOW_CLOCK ? ", Uhr an" : ", Uhr aus";
    s += SHOW_PLATFORM ? ", Steig an" : ", Steig aus";
    return s;
}

}  // namespace board_text
