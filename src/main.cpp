#include "raylib.h"
#include "CameraController.h"
#include "City.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static CameraController* gCamera = nullptr;
static City* gCity = nullptr;

static void UpdateDrawFrame() {
    float dt = GetFrameTime();
    gCamera->Update(dt);
    gCity->Update(dt, gCamera->GetCamera());

    BeginDrawing();
    ClearBackground(Color{151, 183, 205, 255});

    BeginMode3D(gCamera->GetCamera());
    gCity->Draw3D(gCamera->GetCamera());
    EndMode3D();

    gCity->DrawUI();
    EndDrawing();
}

int main() {
#if defined(PLATFORM_WEB)
    // MSAA is expensive in WebGL. Keep the browser build fast by default.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
#endif

    InitWindow(1280, 720, "City Lab C++ v0.6 Progress + Smooth World");
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
