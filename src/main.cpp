#include "raylib.h"
#include "CameraController.h"
#include "City.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif
static CameraController* gCamera=nullptr; static City* gCity=nullptr;
static void UpdateDrawFrame(){float dt=GetFrameTime();gCamera->Update(dt);gCity->Update(dt,gCamera->GetCamera());BeginDrawing();ClearBackground(Color{23,33,29,255});BeginMode3D(gCamera->GetCamera());gCity->Draw3D(gCamera->GetCamera());EndMode3D();gCity->DrawUI();EndDrawing();}
int main(){SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);InitWindow(1280,720,"City Lab C++ v0.3");SetTargetFPS(60);CameraController camera;City city;gCamera=&camera;gCity=&city;
#if defined(PLATFORM_WEB)
emscripten_set_main_loop(UpdateDrawFrame,0,1);
#else
while(!WindowShouldClose())UpdateDrawFrame();
#endif
CloseWindow();return 0;}
