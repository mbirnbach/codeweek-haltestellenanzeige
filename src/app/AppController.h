#pragma once

#include "api/DepartureApi.h"
#include "app/AppState.h"
#include "config/ConfigPortal.h"
#include "config/ConfigStore.h"
#include "display/Display.h"
#include "network/TimeSync.h"
#include "network/WifiManager.h"

// Ties everything together: decides what state we're in, when to fetch,
// and what the display shows. Non-blocking except for the HTTP request
// itself (max. API timeout).
class AppController {
public:
    explicit AppController(Display& display);

    void begin();
    void loop();

    // Reconfiguration, independent of how it was triggered (BOOT button,
    // touch gesture, Serial command).
    void openSetup();           // keep config, restart into the setup portal
    void factoryReset();        // clear config, restart into the setup portal

private:
    void setState(AppState s);
    void startNormalOperation();
    void loopNormal();
    void fetchNow();
    void onFetchFailed(const ApiResult& r);
    void render(bool force = false);
    void showErrorScreen();
    void checkSetupTriggers();
    void handleSerialCommand();
    BoardStatus boardStatus() const;
    bool hasData() const { return lastSuccessMs_ != 0; }

    Display& display_;
    ConfigStore store_;
    DepartureApi api_;
    WifiManager wifi_;
    TimeSync time_;
    ConfigPortal portal_;

    DeviceConfig config_;
    AppState state_ = AppState::BOOTING;

    StationBoard board_;
    ApiResult lastError_;
    uint32_t lastSuccessMs_ = 0;  // 0 = no data yet
    uint32_t nextFetchMs_ = 0;
    uint32_t retryDelayMs_ = 0;
    int failedFetches_ = 0;

    DisplayModel shownModel_;
    bool boardShown_ = false;
    String shownScreenKey_;  // avoids re-drawing identical status screens
    uint32_t lastRenderMs_ = 0;

    uint32_t buttonDownSinceMs_ = 0;
    String serialLine_;
};
