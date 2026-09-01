#pragma once
#include "raylib.h"
#include <vector>

constexpr int GRID_W = 80;
constexpr int GRID_H = 80;
constexpr float CELL_SIZE = 2.0f;
constexpr float WATER_LEVEL = 0.12f;

enum class Tool { Road = 0, Residential = 1, Commercial = 2, Industrial = 3, Bulldoze = 4 };
enum class Zone { None, Residential, Commercial, Industrial };

struct Building {
    int x = 0;
    int z = 0;
    int level = 1;
    int variant = 0;
    Zone zone = Zone::None;
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
    float TerrainHeight(int x, int z) const;
    bool IsWater(int x, int z) const;
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

    void DrawTerrain() const;
    void DrawRoads() const;
    void DrawBuildings() const;
    void DrawBuilding(const Building& b) const;
    void DrawResidential(const Building& b, Vector3 p) const;
    void DrawCommercial(const Building& b, Vector3 p) const;
    void DrawIndustrial(const Building& b, Vector3 p) const;
    void DrawWindows(Vector3 center, float width, float height, float depth, int floors, int columns, bool frontBack) const;

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
