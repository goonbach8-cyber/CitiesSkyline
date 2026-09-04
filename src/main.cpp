#include "raylib.h"
#include "CameraController.h"
#include "City.h"
#include <algorithm>
#include <cmath>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

static CameraController* gCamera = nullptr;
static City* gCity = nullptr;

#if defined(PLATFORM_WEB)
static void SyncWebCanvasSize() {
    double cssW = 0.0, cssH = 0.0;
    if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) != EMSCRIPTEN_RESULT_SUCCESS) return;

    double dpr = emscripten_get_device_pixel_ratio();
    dpr = std::max(1.0, std::min(1.75, dpr));

    int targetW = std::max(640, (int)round(cssW * dpr));
    int targetH = std::max(360, (int)round(cssH * dpr));
    if (GetScreenWidth() != targetW || GetScreenHeight() != targetH) {
        SetWindowSize(targetW, targetH);
    }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void CitySetTool(int tool) { if (gCity) gCity->SelectToolExternal(tool); }
EMSCRIPTEN_KEEPALIVE void CitySetSpeed(int speed) { if (gCity) gCity->SetGameSpeedExternal(speed); }
EMSCRIPTEN_KEEPALIVE int CityMoney() { return gCity ? gCity->Money() : 0; }
EMSCRIPTEN_KEEPALIVE int CityPopulation() { return gCity ? gCity->Population() : 0; }
EMSCRIPTEN_KEEPALIVE int CityFilledJobs() { return gCity ? gCity->FilledJobs() : 0; }
EMSCRIPTEN_KEEPALIVE int CityTotalJobs() { return gCity ? gCity->TotalJobs() : 0; }
EMSCRIPTEN_KEEPALIVE int CityNetMonthly() { return gCity ? gCity->NetMonthly() : 0; }
EMSCRIPTEN_KEEPALIVE int CityPowerUsed() { return gCity ? gCity->PowerUsed() : 0; }
EMSCRIPTEN_KEEPALIVE int CityPowerCapacity() { return gCity ? gCity->PowerCapacity() : 0; }
EMSCRIPTEN_KEEPALIVE int CityWaterUsed() { return gCity ? gCity->WaterUsed() : 0; }
EMSCRIPTEN_KEEPALIVE int CityWaterCapacity() { return gCity ? gCity->WaterCapacity() : 0; }
EMSCRIPTEN_KEEPALIVE int CityHappiness() { return gCity ? gCity->HappinessPercent() : 0; }
EMSCRIPTEN_KEEPALIVE int CityResidentialDemand() { return gCity ? gCity->ResidentialDemandPercent() : 0; }
EMSCRIPTEN_KEEPALIVE int CityCommercialDemand() { return gCity ? gCity->CommercialDemandPercent() : 0; }
EMSCRIPTEN_KEEPALIVE int CityIndustrialDemand() { return gCity ? gCity->IndustrialDemandPercent() : 0; }
EMSCRIPTEN_KEEPALIVE int CityLevel() { return gCity ? gCity->CityLevel() : 1; }
EMSCRIPTEN_KEEPALIVE int CityScore() { return gCity ? gCity->CityScore() : 0; }
EMSCRIPTEN_KEEPALIVE int CityMilestoneTarget() { return gCity ? gCity->MilestoneTarget() : 0; }
EMSCRIPTEN_KEEPALIVE int CitySpeed() { return gCity ? gCity->GameSpeed() : 0; }
EMSCRIPTEN_KEEPALIVE int CitySelectedTool() { return gCity ? gCity->SelectedTool() : 0; }
EMSCRIPTEN_KEEPALIVE int CityVehicleCount() { return gCity ? gCity->VehicleCount() : 0; }
EMSCRIPTEN_KEEPALIVE int CityDay() { return gCity ? gCity->Day() : 1; }
EMSCRIPTEN_KEEPALIVE int CityMonth() { return gCity ? gCity->Month() : 1; }
EMSCRIPTEN_KEEPALIVE int CityYear() { return gCity ? gCity->Year() : 2026; }
}
#endif

static void UpdateDrawFrame() {
#if defined(PLATFORM_WEB)
    SyncWebCanvasSize();
#endif
    float dt = GetFrameTime();
    gCamera->Update(dt);
    gCity->Update(dt, gCamera->GetCamera());

    BeginDrawing();
    ClearBackground(Color{151, 183, 205, 255});

    BeginMode3D(gCamera->GetCamera());
    gCity->Draw3D(gCamera->GetCamera());
    EndMode3D();

#if !defined(PLATFORM_WEB)
    gCity->DrawUI();
#endif
    EndDrawing();
}

int main() {
#if defined(PLATFORM_WEB)
    // MSAA is expensive in WebGL. Keep the browser build fast by default.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
#endif

    InitWindow(1280, 720, "City Lab C++ v0.15 Road-aware Zoning");
    SetTargetFPS(60);

    // IMPORTANT: Keep these out of the WebAssembly stack.
    // City contains large terrain/cache arrays in v0.4.1+, which can overflow
    // Emscripten's stack when City is created as a normal local variable.
    static CameraController camera;
    static City city;

    gCamera = &camera;
    gCity = &city;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif

    CloseWindow();
    return 0;
}
