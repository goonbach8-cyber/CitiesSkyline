
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

struct RoadCell {
    int x;
    int z;
};

struct Building {
    Vector3 position;
    Vector3 size;
    Color color;
    int residents;
    int jobs;
};

static const int GRID_W = 60;
static const int GRID_H = 60;
static const float CELL = 2.0f;

static bool roadGrid[GRID_H][GRID_W] = {};
static std::vector<Building> buildings;

static int money = 100000;
static int population = 0;
static int jobs = 0;
static bool paused = false;
static float simTimer = 0.0f;

static Camera3D camera = {0};
static float cameraYaw = 45.0f;
static float cameraPitch = 50.0f;
static float cameraDistance = 30.0f;
static Vector3 cameraTarget = {0.0f, 0.0f, 0.0f};

static int selectedTool = 0; // 0 road, 1 residential, 2 commercial, 3 industrial
static bool dragging = false;
static Vector2 dragStart = {0,0};

bool IsInsideGrid(int x, int z) {
    return x >= 0 && x < GRID_W && z >= 0 && z < GRID_H;
}

Vector3 GridToWorld(int x, int z) {
    return Vector3{
        (x - GRID_W/2) * CELL + CELL*0.5f,
        0.0f,
        (z - GRID_H/2) * CELL + CELL*0.5f
    };
}

bool IsAdjacentToRoad(int x, int z) {
    const int dx[4] = {1,-1,0,0};
    const int dz[4] = {0,0,1,-1};
    for(int i=0;i<4;i++) {
        int nx=x+dx[i], nz=z+dz[i];
        if(IsInsideGrid(nx,nz) && roadGrid[nz][nx]) return true;
    }
    return false;
}

bool BuildingExistsAt(int x, int z) {
    Vector3 p = GridToWorld(x,z);
    for(const auto& b: buildings) {
        if (fabsf(b.position.x - p.x) < 0.1f && fabsf(b.position.z - p.z) < 0.1f) return true;
    }
    return false;
}

void PlaceRoadLine(int x0, int z0, int x1, int z1) {
    int dx = abs(x1-x0), dz = abs(z1-z0);
    if (dx >= dz) {
        int start = std::min(x0,x1), end = std::max(x0,x1);
        for(int x=start;x<=end;x++) {
            if(!IsInsideGrid(x,z0)) continue;
            if(!roadGrid[z0][x] && money >= 120) {
                roadGrid[z0][x] = true;
                money -= 120;
            }
        }
    } else {
        int start = std::min(z0,z1), end = std::max(z0,z1);
        for(int z=start;z<=end;z++) {
            if(!IsInsideGrid(x0,z)) continue;
            if(!roadGrid[z][x0] && money >= 120) {
                roadGrid[z][x0] = true;
                money -= 120;
            }
        }
    }
}

void PlaceBuilding(int x, int z, int type) {
    if(!IsInsideGrid(x,z) || roadGrid[z][x] || BuildingExistsAt(x,z)) return;
    if(!IsAdjacentToRoad(x,z)) return;

    int price = type==1 ? 400 : 550;
    if(money < price) return;
    money -= price;

    Building b;
    b.position = GridToWorld(x,z);
    b.position.y = 0.65f;
    b.size = {1.5f, 1.3f + (float)GetRandomValue(0, 8)/10.0f, 1.5f};
    b.residents = 0;
    b.jobs = 0;

    if(type == 1) {
        b.color = Color{125, 176, 132, 255};
        b.residents = GetRandomValue(6, 12);
    } else if(type == 2) {
        b.color = Color{95, 145, 200, 255};
        b.jobs = GetRandomValue(6, 10);
    } else {
        b.color = Color{191, 155, 83, 255};
        b.jobs = GetRandomValue(10, 16);
    }

    buildings.push_back(b);
}

void RecalcStats() {
    population = 0;
    jobs = 0;
    for(const auto& b: buildings) {
        population += b.residents;
        jobs += b.jobs;
    }
}

void UpdateCameraOrbit() {
    float yaw = DEG2RAD * cameraYaw;
    float pitch = DEG2RAD * cameraPitch;

    Vector3 offset = {
        cameraDistance * cosf(pitch) * sinf(yaw),
        cameraDistance * sinf(pitch),
        cameraDistance * cosf(pitch) * cosf(yaw)
    };

    camera.position = Vector3Add(cameraTarget, offset);
    camera.target = cameraTarget;
}

bool MouseToGrid(int& gx, int& gz) {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.0001f) return false;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0) return false;

    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    gx = (int)floorf(hit.x / CELL + GRID_W/2.0f);
    gz = (int)floorf(hit.z / CELL + GRID_H/2.0f);
    return IsInsideGrid(gx,gz);
}

void DrawGrid() {
    Color grassA = Color{47, 79, 54, 255};
    Color grassB = Color{50, 84, 58, 255};

    for(int z=0;z<GRID_H;z++) {
        for(int x=0;x<GRID_W;x++) {
            Vector3 p = GridToWorld(x,z);
            Color c = ((x+z)%2==0)?grassA:grassB;

            if(roadGrid[z][x]) {
                DrawCube({p.x, 0.05f, p.z}, CELL*0.98f, 0.1f, CELL*0.98f, Color{70,70,70,255});
                DrawCubeWires({p.x, 0.06f, p.z}, CELL*0.98f, 0.1f, CELL*0.98f, Color{95,95,95,255});
            } else {
                DrawCube({p.x, -0.03f, p.z}, CELL*0.98f, 0.06f, CELL*0.98f, c);
            }
        }
    }
}

void DrawBuildings() {
    for(const auto& b: buildings) {
        DrawCube(b.position, b.size.x, b.size.y, b.size.z, b.color);
        DrawCubeWires(b.position, b.size.x, b.size.y, b.size.z, Color{35,35,35,255});
    }
}

void DrawUI() {
    DrawRectangle(0,0,GetScreenWidth(),54,Color{20,22,21,245});
    DrawText("Cities Prototype C++",18,14,22,RAYWHITE);

    std::string stats = "Geld: " + std::to_string(money) +
                        "   Einwohner: " + std::to_string(population) +
                        "   Jobs: " + std::to_string(jobs);
    DrawText(stats.c_str(),360,17,18,Color{215,220,216,255});

    DrawRectangle(12,72,185,245,Color{18,20,19,235});
    DrawText("BAUEN",24,86,18,RAYWHITE);

    const char* labels[4]={"1 Strasse","2 Wohnen","3 Gewerbe","4 Industrie"};
    for(int i=0;i<4;i++) {
        Rectangle r{22.0f,116.0f+i*46.0f,160.0f,36.0f};
        Color bg = selectedTool==i ? Color{64,83,70,255} : Color{39,44,41,255};
        DrawRectangleRounded(r,0.15f,6,bg);
        DrawText(labels[i],34,(int)r.y+9,16,RAYWHITE);
    }

    DrawText("WASD: Kamera",22,330,15,Color{185,190,186,255});
    DrawText("Q/E: Drehen",22,350,15,Color{185,190,186,255});
    DrawText("Mausrad: Zoom",22,370,15,Color{185,190,186,255});
    DrawText("Ziehen: Strasse",22,390,15,Color{185,190,186,255});
    DrawText("Klick: Gebaeude",22,410,15,Color{185,190,186,255});
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Cities Prototype C++");
    SetTargetFPS(60);

    camera.up = {0.0f,1.0f,0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    cameraTarget = {0.0f,0.0f,0.0f};
    UpdateCameraOrbit();

    while(!WindowShouldClose()) {
        float dt = GetFrameTime();

        if(IsKeyPressed(KEY_ONE)) selectedTool=0;
        if(IsKeyPressed(KEY_TWO)) selectedTool=1;
        if(IsKeyPressed(KEY_THREE)) selectedTool=2;
        if(IsKeyPressed(KEY_FOUR)) selectedTool=3;
        if(IsKeyPressed(KEY_SPACE)) paused = !paused;

        float panSpeed = 20.0f * dt * (cameraDistance/25.0f);
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        forward.y = 0;
        forward = Vector3Normalize(forward);
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

        if(IsKeyDown(KEY_W)) cameraTarget = Vector3Add(cameraTarget, Vector3Scale(forward, panSpeed));
        if(IsKeyDown(KEY_S)) cameraTarget = Vector3Subtract(cameraTarget, Vector3Scale(forward, panSpeed));
        if(IsKeyDown(KEY_A)) cameraTarget = Vector3Subtract(cameraTarget, Vector3Scale(right, panSpeed));
        if(IsKeyDown(KEY_D)) cameraTarget = Vector3Add(cameraTarget, Vector3Scale(right, panSpeed));

        if(IsKeyDown(KEY_Q)) cameraYaw -= 55.0f*dt;
        if(IsKeyDown(KEY_E)) cameraYaw += 55.0f*dt;

        float wheel = GetMouseWheelMove();
        if(wheel != 0) cameraDistance = Clamp(cameraDistance - wheel*3.0f, 8.0f, 65.0f);

        UpdateCameraOrbit();

        int gx,gz;
        bool validCell = MouseToGrid(gx,gz);

        if(selectedTool==0) {
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && validCell) {
                dragging = true;
                dragStart = {(float)gx,(float)gz};
            }
            if(IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && dragging && validCell) {
                PlaceRoadLine((int)dragStart.x,(int)dragStart.y,gx,gz);
                dragging = false;
            }
        } else {
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && validCell) {
                PlaceBuilding(gx,gz,selectedTool);
            }
        }

        if(!paused) {
            simTimer += dt;
            if(simTimer >= 1.0f) {
                simTimer = 0.0f;
                money += population*2 + jobs*3;
                RecalcStats();
            }
        }

        RecalcStats();

        BeginDrawing();
        ClearBackground(Color{14,17,15,255});

        BeginMode3D(camera);
        DrawGrid();
        DrawBuildings();

        if(validCell) {
            Vector3 p = GridToWorld(gx,gz);
            DrawCubeWires({p.x,0.15f,p.z}, CELL*0.95f, 0.2f, CELL*0.95f, YELLOW);
        }
        EndMode3D();

        DrawUI();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
