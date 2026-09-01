#pragma once
#include "raylib.h"
#include <vector>
#include <string>

constexpr int GRID_W = 64;
constexpr int GRID_H = 64;
constexpr float CELL_SIZE = 2.0f;

enum class Tool { Road = 0, Residential = 1, Commercial = 2, Industrial = 3, Bulldoze = 4 };
enum class Zone { None, Residential, Commercial, Industrial };

struct Building {
    int x = 0;
    int z = 0;
    int level = 1;
    Zone zone = Zone::None;
    Vector3 size{1.45f, 1.5f, 1.45f};
    int residents = 0;
    int jobs = 0;
};

class City {
public:
    City();

    void Update(float dt, const Camera3D& camera);
    void Draw3D(const Camera3D& camera) const;
    void DrawUI() const;

private:
    bool Inside(int x, int z) const;
    Vector3 GridToWorld(int x, int z) const;
    bool MouseToGrid(const Camera3D& camera, int& gx, int& gz) const;
    bool AdjacentToRoad(int x, int z) const;
    bool HasBuilding(int x, int z) const;
    int BuildingIndexAt(int x, int z) const;

    void HandleInput(const Camera3D& camera);
    void PlaceRoadLine(int x0, int z0, int x1, int z1);
    void PaintZoneRect(int x0, int z0, int x1, int z1, Zone zone);
    void BulldozeAt(int x, int z);
    void Simulate(float dt);
    void TrySpawnBuilding();
    void RecalculateStats();
    void RecalculateDemand();

    bool roads_[GRID_H][GRID_W]{};
    Zone zones_[GRID_H][GRID_W]{};
    std::vector<Building> buildings_;

    Tool tool_ = Tool::Road;
    bool dragging_ = false;
    int dragStartX_ = 0;
    int dragStartZ_ = 0;
    int hoverX_ = -1;
    int hoverZ_ = -1;

    int money_ = 100000;
    int population_ = 0;
    int jobs_ = 0;
    int roadCount_ = 0;
    float residentialDemand_ = 0.80f;
    float commercialDemand_ = 0.45f;
    float industrialDemand_ = 0.55f;
    float simAccumulator_ = 0.0f;
    float spawnAccumulator_ = 0.0f;
    bool paused_ = false;
};
