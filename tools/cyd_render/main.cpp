// Renders the CYD screens on the host with the real CYDRenderer and Adafruit
// GFX, into 320x240 PPM images. Used by tools/render_cyd.sh to check the
// layout and the participant options without flashing a board.
//
//   cyd_render <out-dir> <live.json> <edge-cases.json>

#include <Adafruit_GFX.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "api/DepartureParser.h"
#include "app/DisplayModelBuilder.h"
#include "display/CYDRenderer.h"
#include "util/TimeUtil.h"

namespace {

StationBoard load(const char* path) {
    std::ifstream in(path);
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    StationBoard b;
    if (!parser::parseDepartures(json.c_str(), json.size(), b).ok()) {
        std::fprintf(stderr, "cannot parse %s\n", path);
        std::exit(1);
    }
    return b;
}

void savePpm(GFXcanvas16& c, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", c.width(), c.height());
    const uint16_t* px = c.getBuffer();
    for (int i = 0; i < c.width() * c.height(); ++i) {
        const uint16_t p = px[i];
        const unsigned char rgb[3] = {static_cast<unsigned char>(((p >> 11) & 0x1F) * 255 / 31),
                                      static_cast<unsigned char>(((p >> 5) & 0x3F) * 255 / 63),
                                      static_cast<unsigned char>((p & 0x1F) * 255 / 31)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <out-dir> <live.json> <edge-cases.json>\n", argv[0]);
        return 1;
    }
    setenv("TZ", timeutil::TZ_EUROPE_BERLIN, 1);
    tzset();
    const std::string out = argv[1];

    GFXcanvas16 screen(320, 240);  // stands in for the ILI9341 after setRotation(1)
    CYDRenderer r(screen);
    r.begin();

    const StationBoard live = load(argv[2]);
    r.showDepartures(buildDisplayModel(live, live.servedAt, BoardStatus::Live));
    savePpm(screen, out + "/board.ppm");

    // Message ticker after ~2.4 s of scrolling (60 steps).
    for (int i = 0; i < 60; ++i) r.scrollTicker();
    savePpm(screen, out + "/board_ticker.ppm");

    // Two minutes later: only the changed rows get redrawn (incremental path).
    r.showDepartures(buildDisplayModel(live, live.servedAt + 120, BoardStatus::Live));
    savePpm(screen, out + "/board_later.ppm");

    const StationBoard edge = load(argv[3]);
    r.showDepartures(buildDisplayModel(edge, edge.servedAt, BoardStatus::Stale));
    for (int i = 0; i < 90; ++i) r.scrollTicker();
    savePpm(screen, out + "/board_stale_cancelled.ppm");

    SetupScreen setup;
    setup.apName = "Abfahrt-Setup-3F2A";
    setup.url = "http://192.168.4.1";
    setup.step = "WLAN ok - Haltestelle wählen";
    r.showSetup(setup);
    savePpm(screen, out + "/setup.ppm");

    StatusScreen err;
    err.isError = true;
    err.title = "Kein WLAN";
    err.detail = "\"Schule-Gast\" nicht erreichbar";
    err.hint = "BOOT-Taste 3 s halten = Einrichtung";
    r.showStatus(err);
    savePpm(screen, out + "/error.ppm");
    return 0;
}
