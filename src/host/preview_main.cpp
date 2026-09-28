// Host preview (PlatformIO env "preview"): renders a departures JSON exactly
// like the Serial display of the ESP32, using the same parser, model builder,
// participant settings and formatter. No ESP32 needed.
//
//   .pio/build/preview/program [--no-color] [--parsed] [--now <epoch>] [file.json]
//
// Without a file, the JSON is read from stdin. tools/preview.sh fetches live
// data from the API and pipes it in.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>
#include <fstream>
#include <string>

#include "api/DepartureParser.h"
#include "app/DisplayModelBuilder.h"
#include "display/BoardFormatter.h"
#include "util/TimeUtil.h"

int main(int argc, char** argv) {
    bool ansi = true;
    bool showParsed = false;
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    const char* file = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--no-color")) ansi = false;
        else if (!std::strcmp(argv[i], "--parsed")) showParsed = true;
        else if (!std::strcmp(argv[i], "--now") && i + 1 < argc) now = std::atoll(argv[++i]);
        else file = argv[i];
    }

    setenv("TZ", timeutil::TZ_EUROPE_BERLIN, 1);
    tzset();

    std::string json;
    if (file) {
        std::ifstream in(file);
        if (!in) {
            std::fprintf(stderr, "cannot open %s\n", file);
            return 1;
        }
        json.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } else {
        json.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }

    StationBoard board;
    const ApiResult r = parser::parseDepartures(json.c_str(), json.size(), board);
    if (!r.ok()) {
        const String code = parser::parseErrorCode(json.c_str(), json.size());
        std::fprintf(stderr, "could not parse departures (API error code: '%s')\n", code.c_str());
        return 1;
    }

    if (showParsed) {
        std::printf("stop: %s (%s)  stale=%d  servedAt=%s\n", board.stop.name.c_str(), board.stop.id.c_str(),
                    board.stale, timeutil::formatClock(board.servedAt).c_str());
        for (const Departure& d : board.departures) {
            std::printf("  %-4s %-32s sched %s  est %s  plat %-2s rt=%d cancelled=%d %s\n", d.line.c_str(),
                        d.destination.c_str(), timeutil::formatClock(d.scheduled).c_str(),
                        d.estimated ? timeutil::formatClock(d.estimated).c_str() : "--:--", d.platform.c_str(),
                        d.realtime, d.cancelled, d.firstMessage.c_str());
        }
        std::printf("\n");
    }

    std::printf("%s\n", board_text::designSummary().c_str());
    const BoardStatus status = board.stale ? BoardStatus::Stale : BoardStatus::Live;
    const DisplayModel model = buildDisplayModel(board, now, status);
    for (const String& line : board_text::departures(model, ansi)) std::printf("%s\n", line.c_str());
    return 0;
}
