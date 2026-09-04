#pragma once

#include "raylib.h"
#include "GameTypes.h"
#include <vector>

constexpr int GRID_W = 92;
constexpr int GRID_H = 92;
constexpr float CELL_SIZE = 2.05f;
constexpr float WATER_LEVEL = 0.16f;

class City {
public:
    City();
    ~City();
    void Update(float dt, const Camera3D& camera);
    void Draw3D(const Camera3D& camera) const;
    void DrawUI() const;

    // Lightweight bridge used by the browser HUD.
    void SelectToolExternal(int tool);
    void SetGameSpeedExternal(int speed);
    int Money() const { return money_; }
    int Population() const { return population_; }
    int FilledJobs() const { return filledJobs_; }
    int TotalJobs() const { return totalJobs_; }
    int NetMonthly() const { return projectedIncome_ - projectedExpenses_; }
    int PowerUsed() const { return powerUsed_; }
    int PowerCapacity() const { return powerCapacity_; }
    int WaterUsed() const { return waterUsed_; }
    int WaterCapacity() const { return waterCapacity_; }
    int HappinessPercent() const { return (int)(happiness_ * 100.0f); }
    int ResidentialDemandPercent() const { return (int)(residentialDemand_ * 100.0f); }
    int CommercialDemandPercent() const { return (int)(commercialDemand_ * 100.0f); }
    int IndustrialDemandPercent() const { return (int)(industrialDemand_ * 100.0f); }
    int CityLevel() const { return cityLevel_; }
    int CityScore() const { return cityScore_; }
    int MilestoneTarget() const { return milestoneTarget_; }
    int GameSpeed() const { return gameSpeed_; }
    int SelectedTool() const { return (int)tool_; }
    int VehicleCount() const { return (int)vehicles_.size(); }
    int Day() const { return day_; }
    int Month() const { return month_; }
    int Year() const { return year_; }

private:
    struct NaturalProp {
        int x = 0;
        int z = 0;
        float scale = 1.0f;
        bool rock = false;
        int seed = 0;
    };

    struct RoadVisualPath {
        std::vector<Vector3> points;
    };

    struct Vehicle {
        int pathIndex = -1;
        int segmentIndex = 0;
        int direction = 1;
        float t = 0.0f;
        float speed = 4.2f; // world units per second
        float laneOffset = 0.16f;
        Color color{190, 70, 65, 255};
    };

    bool Inside(int x, int z) const;
    float ComputeTerrainHeight(int x, int z) const;
    float TerrainHeight(int x, int z) const;
    float TerrainHeightAtWorld(float wx, float wz) const;
    bool IsWater(int x, int z) const;
    bool IsShore(int x, int z) const;
    Vector3 GridToWorld(int x, int z) const;
    Vector3 LotCenter(int x, int z, int w, int d) const;
    float LotGroundHeight(int x, int z, int w, int d) const;
    bool MouseToWorld(const Camera3D& camera, Vector3& hit) const;
    bool MouseToGrid(const Camera3D& camera, int& gx, int& gz) const;

    unsigned int HashCell(int x, int z, int salt = 0) const;
    float Hash01(int x, int z, int salt = 0) const;

    bool RoadAt(int x, int z) const;
    float DistanceToVisibleRoad(float wx, float wz, Vector3* nearest = nullptr, Vector3* tangent = nullptr) const;
    bool CellOverlapsVisibleRoad(int x, int z, float extraMargin = 0.0f) const;
    bool ZoneCellEligible(int x, int z) const;
    bool RoadConnected(int x, int z, int nx, int nz) const;
    void LinkRoadCells(int x, int z, int nx, int nz);
    void UnlinkRoadCell(int x, int z);
    bool BuildingContains(const Building& b, int x, int z) const;
    int BuildingIndexAt(int x, int z) const;
    int ServiceIndexAt(int x, int z) const;
    bool CellBlocked(int x, int z) const;
    bool LotClearForZone(int x, int z, int w, int d, Zone zone) const;
    bool LotClearForService(int x, int z, int w, int d) const;
    bool LotAdjacentToRoad(int x, int z, int w, int d) const;
    int BestRoadOrientation(int x, int z, int w, int d) const;

    bool IsCoveredBy(ServiceKind kind, int x, int z, int w = 1, int d = 1) const;
    float ParkScoreAt(int x, int z) const;
    float PollutionAt(int x, int z) const;

    void HandleInput(const Camera3D& camera);
    void HandleUIInput();
    void SetTool(Tool tool);
    void PlaceRoadLine(int x0, int z0, int x1, int z1);
    void PlaceDraggedRoad();
    std::vector<Vector3> BuildDraggedRoadCurve() const;
    bool RoadCurvePointValid(const Vector3& p) const;
    void PaintZoneRect(int x0, int z0, int x1, int z1, Zone zone);
    void BulldozeAt(int x, int z);
    void PlaceService(ServiceKind kind, int x, int z);

    void Simulate(float dt);
    void DailyTick();
    void MonthlyTick();
    void TrySpawnBuildings(int attempts);
    bool TrySpawnBuildingAt(int x, int z, Zone zone);
    void UpgradeAndOccupancyTick();
    void UpdateServices();
    void UpdateBuildingValues();
    void RecalculateStats();
    void RecalculateDemand();
    void RecalculateBudgetPreview();
    void CheckMilestones();
    void UpdateTraffic(float dt);
    void SpawnVehicle();

    void BuildTerrainCache();
    void BuildTerrainModels();
    void GenerateNaturalProps();
    Model BuildCellSurfaceModel(bool waterCells) const;
    void DrawTerrain() const;
    void DrawNaturalProps(const Camera3D& camera) const;
    void DrawRoads(const Camera3D& camera) const;
    void DrawRoadTile(int x, int z, bool detailed) const;
    void DrawStreetLight(Vector3 p, bool alongX) const;
    void DrawZones(const Camera3D& camera) const;
    void DrawBuildings(const Camera3D& camera) const;
    void DrawServices(const Camera3D& camera) const;
    void DrawTraffic(const Camera3D& camera) const;
    void DrawRoadPreview() const;
    void DrawPlacementPreview() const;

    void DrawTopBar() const;
    void DrawToolPanel() const;
    void DrawDemandPanel() const;
    void DrawInfoPanel() const;
    void DrawProgressPanel() const;
    void DrawBar(int x, int y, int w, float value, Color color) const;
    const char* ToolLabel(Tool tool) const;

    bool roads_[GRID_H][GRID_W]{};
    unsigned char roadLinks_[GRID_H][GRID_W]{};
    Zone zones_[GRID_H][GRID_W]{};
    float terrainHeights_[GRID_H][GRID_W]{};
    bool waterCells_[GRID_H][GRID_W]{};
    bool shoreCells_[GRID_H][GRID_W]{};
    std::vector<Building> buildings_;
    std::vector<ServiceStructure> services_;
    std::vector<NaturalProp> naturalProps_;
    std::vector<RoadVisualPath> roadVisualPaths_;
    std::vector<Vehicle> vehicles_;

    Model landModel_{};
    Model waterModel_{};
    bool terrainModelsReady_ = false;

    Tool tool_ = Tool::Road;
    bool dragging_ = false;
    int dragStartX_ = 0;
    int dragStartZ_ = 0;
    int hoverX_ = -1;
    int hoverZ_ = -1;
    std::vector<Vector3> roadDragWorld_;
    Vector3 roadDragCurrent_{0.0f, 0.0f, 0.0f};
    bool roadDragCurrentValid_ = false;

    int money_ = 60000;
    int population_ = 0;
    int totalJobs_ = 0;
    int filledJobs_ = 0;
    int roadCount_ = 0;
    int powerCapacity_ = 0;
    int powerUsed_ = 0;
    int waterCapacity_ = 0;
    int waterUsed_ = 0;
    int unpoweredBuildings_ = 0;
    int unwateredBuildings_ = 0;

    float residentialDemand_ = 0.72f;
    float commercialDemand_ = 0.36f;
    float industrialDemand_ = 0.46f;
    float happiness_ = 0.58f;
    float unemployment_ = 0.0f;

    int day_ = 1;
    int month_ = 1;
    int year_ = 2026;
    int gameSpeed_ = 1; // 0 paused, 1,2,3
    float dayAccumulator_ = 0.0f;

    int lastIncome_ = 0;
    int lastExpenses_ = 0;
    int projectedIncome_ = 0;
    int projectedExpenses_ = 0;
    int residentialTax_ = 9;
    int commercialTax_ = 10;
    int industrialTax_ = 10;

    // Progression turns the sandbox into a goal-driven city-builder loop.
    int cityLevel_ = 1;
    int cityScore_ = 50;
    int maxBuildingLevel_ = 2;
    int milestoneTarget_ = 100;
    int lastMilestoneReward_ = 0;
    int milestoneBannerDays_ = 0;
    bool financialWarning_ = false;

    float trafficSpawnTimer_ = 0.0f;
    int completedTrips_ = 0;
};
