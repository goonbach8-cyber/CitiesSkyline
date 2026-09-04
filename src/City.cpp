#include "City.h"
#include "BuildingRenderer.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
Color ZoneColor(Zone zone, unsigned char alpha) {
    switch (zone) {
        case Zone::Residential: return Color{76, 193, 111, alpha};
        case Zone::Commercial: return Color{67, 148, 233, alpha};
        case Zone::Industrial: return Color{228, 177, 62, alpha};
        default: return Color{0, 0, 0, 0};
    }
}

Color ShiftColor(Color c, int amount) {
    auto cc = [](int v) { return (unsigned char)std::max(0, std::min(255, v)); };
    return Color{cc(c.r + amount), cc(c.g + amount), cc(c.b + amount), c.a};
}

float DistSq(float ax, float az, float bx, float bz) {
    float dx = ax - bx;
    float dz = az - bz;
    return dx * dx + dz * dz;
}
}

City::City() {
    BuildTerrainCache();
    GenerateNaturalProps();
    BuildTerrainModels();

    // Small starter road so the player immediately has a visible anchor.
    for (int x = 38; x <= 53; ++x) {
        if (!IsWater(x, 48)) {
            roads_[48][x] = true;
            ++roadCount_;
        }
    }
    RecalculateBudgetPreview();
}

City::~City() {
    if (terrainModelsReady_) {
        UnloadModel(landModel_);
        UnloadModel(waterModel_);
    }
}

bool City::Inside(int x, int z) const {
    return x >= 0 && x < GRID_W && z >= 0 && z < GRID_H;
}

unsigned int City::HashCell(int x, int z, int salt) const {
    unsigned int h = (unsigned int)(x * 374761393u + z * 668265263u + salt * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float City::Hash01(int x, int z, int salt) const {
    return (HashCell(x, z, salt) & 0x00FFFFFFu) / 16777215.0f;
}

float City::ComputeTerrainHeight(int x, int z) const {
    float nx = (x - GRID_W * 0.5f) / (GRID_W * 0.5f);
    float nz = (z - GRID_H * 0.5f) / (GRID_H * 0.5f);

    float rolling = 0.26f * sinf(nx * 5.1f)
                  + 0.21f * cosf(nz * 4.4f)
                  + 0.13f * sinf((nx + nz) * 8.2f)
                  + 0.08f * cosf((nx - nz) * 10.5f);

    float edgeHills = 0.82f * powf(std::max(fabsf(nx), fabsf(nz)), 2.6f);
    float ridge = 0.64f * expf(-((nx + 0.56f) * (nx + 0.56f) * 8.0f + (nz - 0.48f) * (nz - 0.48f) * 6.5f));

    float riverPath = -0.40f + 0.10f * sinf(nx * 5.4f) + 0.035f * sinf(nx * 13.0f);
    float river = 1.62f * expf(-powf(nz - riverPath, 2.0f) * 125.0f);

    float lakeDx = nx - 0.52f;
    float lakeDz = nz - 0.16f;
    float lake = 2.10f * expf(-(lakeDx * lakeDx * 12.0f + lakeDz * lakeDz * 9.0f));

    float pondDx = nx + 0.62f;
    float pondDz = nz + 0.22f;
    float pond = 1.16f * expf(-(pondDx * pondDx * 23.0f + pondDz * pondDz * 19.0f));

    return 0.72f + rolling + edgeHills + ridge - river - lake - pond;
}

float City::TerrainHeight(int x, int z) const {
    if (!Inside(x, z)) return 0.0f;
    return terrainHeights_[z][x];
}

float City::TerrainHeightAtWorld(float wx, float wz) const {
    int gx = (int)floorf(wx / CELL_SIZE + GRID_W / 2.0f);
    int gz = (int)floorf(wz / CELL_SIZE + GRID_H / 2.0f);
    gx = std::max(0, std::min(GRID_W - 1, gx));
    gz = std::max(0, std::min(GRID_H - 1, gz));
    return TerrainHeight(gx, gz);
}

bool City::IsWater(int x, int z) const {
    return Inside(x, z) && waterCells_[z][x];
}

bool City::IsShore(int x, int z) const {
    return Inside(x, z) && shoreCells_[z][x];
}

Vector3 City::GridToWorld(int x, int z) const {
    return {
        (x - GRID_W / 2) * CELL_SIZE + CELL_SIZE * 0.5f,
        TerrainHeight(x, z),
        (z - GRID_H / 2) * CELL_SIZE + CELL_SIZE * 0.5f
    };
}

float City::LotGroundHeight(int x, int z, int w, int d) const {
    float sum = 0.0f;
    int count = 0;
    for (int zz = z; zz < z + d; ++zz) {
        for (int xx = x; xx < x + w; ++xx) {
            if (Inside(xx, zz)) {
                sum += TerrainHeight(xx, zz);
                ++count;
            }
        }
    }
    return count > 0 ? sum / count : 0.0f;
}

Vector3 City::LotCenter(int x, int z, int w, int d) const {
    float wx = (x - GRID_W / 2) * CELL_SIZE + CELL_SIZE * w * 0.5f;
    float wz = (z - GRID_H / 2) * CELL_SIZE + CELL_SIZE * d * 0.5f;
    return {wx, LotGroundHeight(x, z, w, d), wz};
}

bool City::MouseToGrid(const Camera3D& camera, int& gx, int& gz) const {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.00001f) return false;

    float targetY = 0.65f;
    float t = (targetY - ray.position.y) / ray.direction.y;
    if (t < 0.0f) return false;

    // Refine intersection against the sampled terrain height. This is much more accurate on hills.
    for (int i = 0; i < 4; ++i) {
        Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        float h = TerrainHeightAtWorld(hit.x, hit.z);
        t = (h - ray.position.y) / ray.direction.y;
        if (t < 0.0f) return false;
    }

    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    gx = (int)floorf(hit.x / CELL_SIZE + GRID_W / 2.0f);
    gz = (int)floorf(hit.z / CELL_SIZE + GRID_H / 2.0f);
    return Inside(gx, gz);
}

bool City::RoadAt(int x, int z) const {
    return Inside(x, z) && roads_[z][x];
}

bool City::BuildingContains(const Building& b, int x, int z) const {
    return x >= b.x && x < b.x + b.w && z >= b.z && z < b.z + b.d;
}

int City::BuildingIndexAt(int x, int z) const {
    for (int i = 0; i < (int)buildings_.size(); ++i) {
        if (BuildingContains(buildings_[i], x, z)) return i;
    }
    return -1;
}

int City::ServiceIndexAt(int x, int z) const {
    for (int i = 0; i < (int)services_.size(); ++i) {
        const auto& s = services_[i];
        if (x >= s.x && x < s.x + s.w && z >= s.z && z < s.z + s.d) return i;
    }
    return -1;
}

bool City::CellBlocked(int x, int z) const {
    if (!Inside(x, z) || IsWater(x, z) || RoadAt(x, z)) return true;
    if (BuildingIndexAt(x, z) >= 0) return true;
    if (ServiceIndexAt(x, z) >= 0) return true;
    return false;
}

bool City::LotClearForZone(int x, int z, int w, int d, Zone zone) const {
    if (!Inside(x, z) || !Inside(x + w - 1, z + d - 1)) return false;
    float minH = 999.0f;
    float maxH = -999.0f;
    for (int zz = z; zz < z + d; ++zz) {
        for (int xx = x; xx < x + w; ++xx) {
            if (zones_[zz][xx] != zone || CellBlocked(xx, zz)) return false;
            float h = TerrainHeight(xx, zz);
            minH = std::min(minH, h);
            maxH = std::max(maxH, h);
        }
    }
    return (maxH - minH) < 0.52f;
}

bool City::LotClearForService(int x, int z, int w, int d) const {
    if (!Inside(x, z) || !Inside(x + w - 1, z + d - 1)) return false;
    float minH = 999.0f;
    float maxH = -999.0f;
    for (int zz = z; zz < z + d; ++zz) {
        for (int xx = x; xx < x + w; ++xx) {
            if (CellBlocked(xx, zz)) return false;
            float h = TerrainHeight(xx, zz);
            minH = std::min(minH, h);
            maxH = std::max(maxH, h);
        }
    }
    return (maxH - minH) < 0.62f;
}

bool City::LotAdjacentToRoad(int x, int z, int w, int d) const {
    for (int xx = x; xx < x + w; ++xx) {
        if (RoadAt(xx, z - 1) || RoadAt(xx, z + d)) return true;
    }
    for (int zz = z; zz < z + d; ++zz) {
        if (RoadAt(x - 1, zz) || RoadAt(x + w, zz)) return true;
    }
    return false;
}

int City::BestRoadOrientation(int x, int z, int w, int d) const {
    int score[4]{0, 0, 0, 0};
    for (int xx = x; xx < x + w; ++xx) {
        if (RoadAt(xx, z - 1)) ++score[0];
        if (RoadAt(xx, z + d)) ++score[2];
    }
    for (int zz = z; zz < z + d; ++zz) {
        if (RoadAt(x + w, zz)) ++score[1];
        if (RoadAt(x - 1, zz)) ++score[3];
    }
    int best = 0;
    for (int i = 1; i < 4; ++i) if (score[i] > score[best]) best = i;
    return best;
}

bool City::IsCoveredBy(ServiceKind kind, int x, int z, int w, int d) const {
    float cx = x + w * 0.5f;
    float cz = z + d * 0.5f;
    for (const auto& s : services_) {
        if (s.kind != kind) continue;
        float sx = s.x + s.w * 0.5f;
        float sz = s.z + s.d * 0.5f;
        if (DistSq(cx, cz, sx, sz) <= s.radius * s.radius) return true;
    }
    return false;
}

float City::ParkScoreAt(int x, int z) const {
    float score = 0.0f;
    for (const auto& s : services_) {
        if (s.kind != ServiceKind::Park) continue;
        float sx = s.x + s.w * 0.5f;
        float sz = s.z + s.d * 0.5f;
        float d2 = DistSq((float)x, (float)z, sx, sz);
        float r2 = s.radius * s.radius;
        if (d2 < r2) score += 1.0f - sqrtf(d2 / r2);
    }
    return Clamp(score, 0.0f, 1.0f);
}

float City::PollutionAt(int x, int z) const {
    float pollution = 0.0f;
    for (const auto& b : buildings_) {
        if (b.zone != Zone::Industrial) continue;
        float cx = b.x + b.w * 0.5f;
        float cz = b.z + b.d * 0.5f;
        float d2 = DistSq((float)x, (float)z, cx, cz);
        float radius = 7.5f + b.level * 1.5f;
        if (d2 < radius * radius) {
            pollution += (1.0f - sqrtf(d2) / radius) * (0.18f + 0.05f * b.level);
        }
    }
    return Clamp(pollution, 0.0f, 1.0f);
}

void City::SetTool(Tool tool) {
    tool_ = tool;
    dragging_ = false;
}

void City::Update(float dt, const Camera3D& camera) {
    HandleInput(camera);
    Simulate(dt);
}

void City::HandleUIInput() {
    Vector2 m = GetMousePosition();
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    // Clickable speed controls.
    int w = GetScreenWidth();
    Rectangle speedRects[4] = {
        {(float)w - 264, 16, 44, 30},
        {(float)w - 214, 16, 44, 30},
        {(float)w - 164, 16, 44, 30},
        {(float)w - 114, 16, 44, 30}
    };
    for (int i = 0; i < 4; ++i) {
        if (CheckCollisionPointRec(m, speedRects[i])) {
            gameSpeed_ = i;
            return;
        }
    }

    // Clickable tool buttons.
    for (int i = 0; i < 8; ++i) {
        Rectangle r{28.0f, 130.0f + i * 37.0f, 210.0f, 31.0f};
        if (CheckCollisionPointRec(m, r)) {
            SetTool((Tool)i);
            return;
        }
    }
}

void City::HandleInput(const Camera3D& camera) {
    if (IsKeyPressed(KEY_ONE)) SetTool(Tool::Road);
    if (IsKeyPressed(KEY_TWO)) SetTool(Tool::Residential);
    if (IsKeyPressed(KEY_THREE)) SetTool(Tool::Commercial);
    if (IsKeyPressed(KEY_FOUR)) SetTool(Tool::Industrial);
    if (IsKeyPressed(KEY_FIVE) || IsKeyPressed(KEY_B)) SetTool(Tool::Bulldoze);
    if (IsKeyPressed(KEY_SIX)) SetTool(Tool::Power);
    if (IsKeyPressed(KEY_SEVEN)) SetTool(Tool::Water);
    if (IsKeyPressed(KEY_EIGHT)) SetTool(Tool::Park);
    if (IsKeyPressed(KEY_SPACE)) gameSpeed_ = gameSpeed_ == 0 ? 1 : 0;

    HandleUIInput();

    int gx = -1, gz = -1;
    if (MouseToGrid(camera, gx, gz)) {
        hoverX_ = gx;
        hoverZ_ = gz;
    } else {
        hoverX_ = hoverZ_ = -1;
    }

    Vector2 m = GetMousePosition();
    if (m.y < 66.0f || (m.x < 256.0f && m.y < 445.0f)) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverX_ >= 0) {
        if (tool_ == Tool::Bulldoze) {
            BulldozeAt(hoverX_, hoverZ_);
        } else if (tool_ == Tool::Power) {
            PlaceService(ServiceKind::PowerPlant, hoverX_, hoverZ_);
        } else if (tool_ == Tool::Water) {
            PlaceService(ServiceKind::WaterTower, hoverX_, hoverZ_);
        } else if (tool_ == Tool::Park) {
            PlaceService(ServiceKind::Park, hoverX_, hoverZ_);
        } else {
            dragging_ = true;
            dragStartX_ = hoverX_;
            dragStartZ_ = hoverZ_;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && dragging_) {
        if (hoverX_ >= 0) {
            if (tool_ == Tool::Road) PlaceRoadLine(dragStartX_, dragStartZ_, hoverX_, hoverZ_);
            else if (tool_ == Tool::Residential) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Residential);
            else if (tool_ == Tool::Commercial) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Commercial);
            else if (tool_ == Tool::Industrial) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Industrial);
        }
        dragging_ = false;
    }
}

void City::PlaceRoadLine(int x0, int z0, int x1, int z1) {
    auto place = [&](int x, int z) {
        if (!Inside(x, z) || RoadAt(x, z) || IsWater(x, z) || money_ < 120) return;
        int bi = BuildingIndexAt(x, z);
        int si = ServiceIndexAt(x, z);
        if (bi >= 0 || si >= 0) return;
        roads_[z][x] = true;
        zones_[z][x] = Zone::None;
        money_ -= 120;
        ++roadCount_;
    };

    int dx = abs(x1 - x0);
    int dz = abs(z1 - z0);
    if (dx >= dz) {
        int a = std::min(x0, x1), b = std::max(x0, x1);
        for (int x = a; x <= b; ++x) place(x, z0);
    } else {
        int a = std::min(z0, z1), b = std::max(z0, z1);
        for (int z = a; z <= b; ++z) place(x0, z);
    }
    RecalculateBudgetPreview();
}

void City::PaintZoneRect(int x0, int z0, int x1, int z1, Zone zone) {
    int xa = std::min(x0, x1), xb = std::max(x0, x1);
    int za = std::min(z0, z1), zb = std::max(z0, z1);
    for (int z = za; z <= zb; ++z) {
        for (int x = xa; x <= xb; ++x) {
            if (!Inside(x, z) || RoadAt(x, z) || IsWater(x, z) || ServiceIndexAt(x, z) >= 0) continue;
            if (BuildingIndexAt(x, z) < 0) zones_[z][x] = zone;
        }
    }
}

void City::BulldozeAt(int x, int z) {
    int bi = BuildingIndexAt(x, z);
    if (bi >= 0) {
        Building b = buildings_[bi];
        for (int zz = b.z; zz < b.z + b.d; ++zz) {
            for (int xx = b.x; xx < b.x + b.w; ++xx) zones_[zz][xx] = b.zone;
        }
        buildings_.erase(buildings_.begin() + bi);
        money_ -= 40;
    } else {
        int si = ServiceIndexAt(x, z);
        if (si >= 0) {
            services_.erase(services_.begin() + si);
            money_ -= 100;
        } else if (RoadAt(x, z)) {
            roads_[z][x] = false;
            roadCount_ = std::max(0, roadCount_ - 1);
            money_ -= 20;
        } else if (Inside(x, z)) {
            zones_[z][x] = Zone::None;
        }
    }
    UpdateServices();
    RecalculateStats();
    RecalculateBudgetPreview();
}

void City::PlaceService(ServiceKind kind, int x, int z) {
    ServiceStructure s;
    s.kind = kind;
    if (kind == ServiceKind::PowerPlant) {
        s.w = 2; s.d = 2; s.radius = 24.0f; s.capacity = 900; s.maintenance = 320;
        if (money_ < 6500) return;
    } else if (kind == ServiceKind::WaterTower) {
        s.w = 1; s.d = 1; s.radius = 22.0f; s.capacity = 720; s.maintenance = 190;
        if (money_ < 3600) return;
    } else {
        s.w = 2; s.d = 2; s.radius = 10.0f; s.capacity = 0; s.maintenance = 85;
        if (money_ < 1200) return;
    }
    s.x = x;
    s.z = z;

    if (!LotClearForService(s.x, s.z, s.w, s.d)) return;
    if (kind != ServiceKind::WaterTower && !LotAdjacentToRoad(s.x, s.z, s.w, s.d)) return;

    for (int zz = s.z; zz < s.z + s.d; ++zz) {
        for (int xx = s.x; xx < s.x + s.w; ++xx) zones_[zz][xx] = Zone::None;
    }

    if (kind == ServiceKind::PowerPlant) money_ -= 6500;
    else if (kind == ServiceKind::WaterTower) money_ -= 3600;
    else money_ -= 1200;

    services_.push_back(s);
    UpdateServices();
    RecalculateBudgetPreview();
}

void City::Simulate(float dt) {
    if (gameSpeed_ <= 0) return;
    float multiplier = gameSpeed_ == 1 ? 1.0f : (gameSpeed_ == 2 ? 2.35f : 4.8f);
    dayAccumulator_ += dt * multiplier;
    while (dayAccumulator_ >= 0.72f) {
        dayAccumulator_ -= 0.72f;
        DailyTick();
    }
}

void City::DailyTick() {
    ++day_;
    if (day_ > 30) {
        day_ = 1;
        ++month_;
        MonthlyTick();
        if (month_ > 12) {
            month_ = 1;
            ++year_;
        }
    }

    UpdateServices();
    UpdateBuildingValues();
    UpgradeAndOccupancyTick();
    RecalculateStats();
    RecalculateDemand();

    int attempts = 2 + (gameSpeed_ >= 2 ? 1 : 0);
    TrySpawnBuildings(attempts);
    UpdateServices();
    RecalculateStats();
    RecalculateBudgetPreview();
}

void City::MonthlyTick() {
    RecalculateBudgetPreview();
    lastIncome_ = projectedIncome_;
    lastExpenses_ = projectedExpenses_;
    money_ += lastIncome_ - lastExpenses_;
}

void City::TrySpawnBuildings(int attempts) {
    for (int n = 0; n < attempts; ++n) {
        for (int tryCell = 0; tryCell < 60; ++tryCell) {
            int x = GetRandomValue(0, GRID_W - 1);
            int z = GetRandomValue(0, GRID_H - 1);
            Zone zone = zones_[z][x];
            if (zone == Zone::None) continue;
            if (TrySpawnBuildingAt(x, z, zone)) break;
        }
    }
}

bool City::TrySpawnBuildingAt(int x, int z, Zone zone) {
    float demand = zone == Zone::Residential ? residentialDemand_ : (zone == Zone::Commercial ? commercialDemand_ : industrialDemand_);
    if (GetRandomValue(0, 1000) / 1000.0f > demand) return false;

    int w = 1, d = 1;
    int roll = GetRandomValue(0, 99);
    if (zone == Zone::Residential) {
        if (roll > 82 && demand > 0.55f) { w = 2; d = 2; }
        else if (roll > 58) { w = 2; d = 1; }
    } else if (zone == Zone::Commercial) {
        if (roll > 78 && population_ > 80) { w = 2; d = 2; }
        else if (roll > 48) { w = 2; d = 1; }
    } else {
        if (roll > 42) { w = 2; d = 2; }
        else { w = 2; d = 1; }
    }

    // Try a few anchor offsets so larger lots can grow around the selected cell.
    const int offsets[4][2] = {{0,0},{-1,0},{0,-1},{-1,-1}};
    for (int oi = 0; oi < 4; ++oi) {
        int ax = x + offsets[oi][0];
        int az = z + offsets[oi][1];
        if (!LotClearForZone(ax, az, w, d, zone)) continue;
        if (!LotAdjacentToRoad(ax, az, w, d)) continue;
        if (!IsCoveredBy(ServiceKind::PowerPlant, ax, az, w, d)) continue;
        if (!IsCoveredBy(ServiceKind::WaterTower, ax, az, w, d)) continue;

        Building b;
        b.x = ax; b.z = az; b.w = w; b.d = d; b.zone = zone;
        b.orientation = BestRoadOrientation(ax, az, w, d);
        b.variant = GetRandomValue(0, zone == Zone::Residential ? 11 : 9);
        b.palette = GetRandomValue(0, zone == Zone::Residential ? 11 : 9);
        b.roofStyle = GetRandomValue(0, 4);
        b.detailSeed = GetRandomValue(1, 1000000);
        b.level = 1;

        int area = w * d;
        if (zone == Zone::Residential) {
            b.capacity = area * GetRandomValue(8, 16);
            b.occupants = std::max(1, b.capacity / 5);
        } else if (zone == Zone::Commercial) {
            b.jobs = area * GetRandomValue(8, 15);
            b.employees = std::max(1, b.jobs / 4);
        } else {
            b.jobs = area * GetRandomValue(12, 22);
            b.employees = std::max(1, b.jobs / 5);
        }

        b.powered = true;
        b.watered = true;
        buildings_.push_back(b);
        return true;
    }
    return false;
}

void City::UpdateServices() {
    powerCapacity_ = 0;
    waterCapacity_ = 0;
    for (const auto& s : services_) {
        if (s.kind == ServiceKind::PowerPlant) powerCapacity_ += s.capacity;
        else if (s.kind == ServiceKind::WaterTower) waterCapacity_ += s.capacity;
    }

    int remainingPower = powerCapacity_;
    int remainingWater = waterCapacity_;
    powerUsed_ = 0;
    waterUsed_ = 0;
    unpoweredBuildings_ = 0;
    unwateredBuildings_ = 0;

    for (auto& b : buildings_) {
        int scale = b.zone == Zone::Residential ? std::max(1, b.occupants) : std::max(1, b.employees);
        int powerNeed = std::max(2, (int)ceilf(scale * (b.zone == Zone::Industrial ? 1.45f : 0.85f)));
        int waterNeed = std::max(2, (int)ceilf(scale * (b.zone == Zone::Industrial ? 0.70f : 1.00f)));

        bool powerCoverage = IsCoveredBy(ServiceKind::PowerPlant, b.x, b.z, b.w, b.d);
        bool waterCoverage = IsCoveredBy(ServiceKind::WaterTower, b.x, b.z, b.w, b.d);

        b.powered = powerCoverage && remainingPower >= powerNeed;
        b.watered = waterCoverage && remainingWater >= waterNeed;

        if (b.powered) { remainingPower -= powerNeed; powerUsed_ += powerNeed; }
        else ++unpoweredBuildings_;
        if (b.watered) { remainingWater -= waterNeed; waterUsed_ += waterNeed; }
        else ++unwateredBuildings_;
    }
}

void City::UpdateBuildingValues() {
    for (auto& b : buildings_) {
        float park = ParkScoreAt(b.x + b.w / 2, b.z + b.d / 2);
        float pollution = PollutionAt(b.x + b.w / 2, b.z + b.d / 2);
        float services = (b.powered ? 0.12f : -0.18f) + (b.watered ? 0.12f : -0.18f);
        float land = 0.38f + park * 0.34f - pollution * (b.zone == Zone::Residential ? 0.40f : 0.14f) + services;
        b.landValue = Clamp(land, 0.05f, 1.0f);
        float happy = 0.48f + park * 0.26f - pollution * 0.33f + (b.powered ? 0.11f : -0.20f) + (b.watered ? 0.11f : -0.20f);
        b.happiness = Clamp(happy, 0.05f, 1.0f);
    }
}

void City::UpgradeAndOccupancyTick() {
    int workforce = std::max(0, (int)(population_ * 0.52f));
    int employeesAllocated = 0;

    // Residential occupancy grows with services, demand and happiness.
    for (auto& b : buildings_) {
        ++b.ageDays;
        if (b.zone != Zone::Residential) continue;
        int target = (int)(b.capacity * Clamp(0.24f + residentialDemand_ * 0.58f + b.happiness * 0.28f, 0.05f, 1.0f));
        if (!b.powered || !b.watered) target = std::min(target, b.capacity / 5);
        if (b.occupants < target) b.occupants += std::min(2 + b.level, target - b.occupants);
        else if (b.occupants > target) b.occupants -= std::min(2, b.occupants - target);
    }

    // Jobs fill from the available workforce.
    for (auto& b : buildings_) {
        if (b.zone == Zone::Residential) continue;
        float demand = b.zone == Zone::Commercial ? commercialDemand_ : industrialDemand_;
        int desired = (int)(b.jobs * Clamp(0.32f + demand * 0.65f, 0.10f, 1.0f));
        if (!b.powered || !b.watered) desired = std::min(desired, b.jobs / 6);
        int available = std::max(0, workforce - employeesAllocated);
        b.employees = std::min(desired, available);
        employeesAllocated += b.employees;
    }

    for (auto& b : buildings_) {
        float fullness = 0.0f;
        if (b.zone == Zone::Residential) fullness = b.capacity > 0 ? (float)b.occupants / b.capacity : 0.0f;
        else fullness = b.jobs > 0 ? (float)b.employees / b.jobs : 0.0f;

        if (b.level < 3 && b.ageDays > 18 && fullness > 0.72f && b.landValue > (b.level == 1 ? 0.48f : 0.64f)) {
            int chance = b.level == 1 ? 12 : 6;
            if (GetRandomValue(0, 99) < chance) {
                ++b.level;
                if (b.zone == Zone::Residential) b.capacity = (int)(b.capacity * 1.48f) + 2;
                else b.jobs = (int)(b.jobs * 1.42f) + 2;
            }
        }
    }
}

void City::RecalculateStats() {
    population_ = 0;
    totalJobs_ = 0;
    filledJobs_ = 0;
    float happinessWeighted = 0.0f;
    int happyWeight = 0;

    for (const auto& b : buildings_) {
        if (b.zone == Zone::Residential) {
            population_ += b.occupants;
            happinessWeighted += b.happiness * std::max(1, b.occupants);
            happyWeight += std::max(1, b.occupants);
        } else {
            totalJobs_ += b.jobs;
            filledJobs_ += b.employees;
        }
    }
    happiness_ = happyWeight > 0 ? happinessWeighted / happyWeight : 0.58f;
    int workforce = std::max(0, (int)(population_ * 0.52f));
    unemployment_ = workforce > 0 ? Clamp((float)std::max(0, workforce - filledJobs_) / workforce, 0.0f, 1.0f) : 0.0f;
}

void City::RecalculateDemand() {
    int workforce = std::max(1, (int)(population_ * 0.52f));
    int vacancies = std::max(0, totalJobs_ - filledJobs_);
    float jobOpportunity = Clamp((float)(vacancies + 18) / (workforce + 30), 0.0f, 1.0f);

    residentialDemand_ = Clamp(0.22f + jobOpportunity * 0.52f + happiness_ * 0.32f - unemployment_ * 0.35f, 0.05f, 0.98f);

    int commercialJobs = 0;
    int industrialJobs = 0;
    for (const auto& b : buildings_) {
        if (b.zone == Zone::Commercial) commercialJobs += b.jobs;
        if (b.zone == Zone::Industrial) industrialJobs += b.jobs;
    }

    float desiredCommercial = population_ * 0.18f + 12.0f;
    commercialDemand_ = Clamp(0.18f + (desiredCommercial - commercialJobs) / 140.0f + happiness_ * 0.16f, 0.05f, 0.95f);

    float desiredIndustry = population_ * 0.24f + 18.0f;
    industrialDemand_ = Clamp(0.18f + (desiredIndustry - industrialJobs) / 165.0f + unemployment_ * 0.34f, 0.05f, 0.95f);
}

void City::RecalculateBudgetPreview() {
    int resTaxBase = population_ * residentialTax_ / 3;
    int commercialEmployees = 0;
    int industrialEmployees = 0;
    for (const auto& b : buildings_) {
        if (b.zone == Zone::Commercial) commercialEmployees += b.employees;
        else if (b.zone == Zone::Industrial) industrialEmployees += b.employees;
    }

    projectedIncome_ = resTaxBase
                     + commercialEmployees * commercialTax_ / 2
                     + industrialEmployees * industrialTax_ / 2;

    projectedExpenses_ = roadCount_ * 5;
    for (const auto& s : services_) projectedExpenses_ += s.maintenance;
}

void City::BuildTerrainCache() {
    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            float h = ComputeTerrainHeight(x, z);
            terrainHeights_[z][x] = h;
            waterCells_[z][x] = h < WATER_LEVEL;
            shoreCells_[z][x] = h >= WATER_LEVEL && h < WATER_LEVEL + 0.22f;
        }
    }
}

void City::GenerateNaturalProps() {
    naturalProps_.clear();
    naturalProps_.reserve(420);
    for (int z = 1; z < GRID_H - 1; ++z) {
        for (int x = 1; x < GRID_W - 1; ++x) {
            if (IsWater(x, z) || IsShore(x, z)) continue;
            float r = Hash01(x, z, 31);
            float h = TerrainHeight(x, z);
            if (r < 0.040f && h < 1.72f) {
                NaturalProp prop;
                prop.x = x;
                prop.z = z;
                prop.scale = 0.72f + Hash01(x, z, 44) * 0.62f;
                prop.rock = false;
                prop.seed = (int)HashCell(x, z, 50);
                naturalProps_.push_back(prop);
            } else if (r > 0.991f && h > 1.15f) {
                NaturalProp prop;
                prop.x = x;
                prop.z = z;
                prop.scale = 0.85f + Hash01(x, z, 73) * 0.45f;
                prop.rock = true;
                prop.seed = (int)HashCell(x, z, 75);
                naturalProps_.push_back(prop);
            }
        }
    }
}

Model City::BuildCellSurfaceModel(bool waterCells) const {
    int cellCount = 0;
    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            if (IsWater(x, z) == waterCells) ++cellCount;
        }
    }

    Mesh mesh{};
    mesh.triangleCount = cellCount * 2;
    mesh.vertexCount = cellCount * 6;
    mesh.vertices = (float*)MemAlloc((size_t)mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = (unsigned char*)MemAlloc((size_t)mesh.vertexCount * 4 * sizeof(unsigned char));

    int vi = 0;
    auto pushVertex = [&](float vx, float vy, float vz, Color c) {
        int vbase = vi * 3;
        mesh.vertices[vbase + 0] = vx;
        mesh.vertices[vbase + 1] = vy;
        mesh.vertices[vbase + 2] = vz;
        int cbase = vi * 4;
        mesh.colors[cbase + 0] = c.r;
        mesh.colors[cbase + 1] = c.g;
        mesh.colors[cbase + 2] = c.b;
        mesh.colors[cbase + 3] = c.a;
        ++vi;
    };

    const float half = CELL_SIZE * 0.5025f;
    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            if (IsWater(x, z) != waterCells) continue;
            Vector3 p = GridToWorld(x, z);
            float y = waterCells ? WATER_LEVEL - 0.045f : p.y - 0.018f;
            Color c;
            if (waterCells) {
                c = ((x + z) & 1) ? Color{50, 119, 159, 255} : Color{55, 126, 166, 255};
            } else if (IsShore(x, z)) {
                c = Color{171, 162, 122, 255};
            } else if (p.y > 1.75f) {
                c = Color{110, 121, 100, 255};
            } else {
                float shade = Clamp((p.y + 0.2f) / 2.5f, 0.0f, 1.0f);
                int checker = ((x + z) & 1) ? 2 : -2;
                c = Color{
                    (unsigned char)(76 + shade * 16 + checker),
                    (unsigned char)(123 + shade * 27 + checker),
                    (unsigned char)(76 + shade * 15 + checker),
                    255
                };
            }

            float x0 = p.x - half, x1 = p.x + half;
            float z0 = p.z - half, z1 = p.z + half;
            pushVertex(x0, y, z0, c); pushVertex(x1, y, z0, c); pushVertex(x1, y, z1, c);
            pushVertex(x0, y, z0, c); pushVertex(x1, y, z1, c); pushVertex(x0, y, z1, c);
        }
    }

    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

void City::BuildTerrainModels() {
    landModel_ = BuildCellSurfaceModel(false);
    waterModel_ = BuildCellSurfaceModel(true);
    terrainModelsReady_ = true;
}

void City::DrawTerrain() const {
    if (!terrainModelsReady_) return;
    DrawModel(landModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    DrawModel(waterModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
}

void City::DrawNaturalProps(const Camera3D& camera) const {
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.65f + 34.0f;
    float detailRadius = cameraDistance * 0.48f + 25.0f;
    float visibleSq = visibleRadius * visibleRadius;
    float detailSq = detailRadius * detailRadius;

    for (const auto& prop : naturalProps_) {
        if (RoadAt(prop.x, prop.z) || zones_[prop.z][prop.x] != Zone::None) continue;
        if (ServiceIndexAt(prop.x, prop.z) >= 0) continue;
        Vector3 p = GridToWorld(prop.x, prop.z);
        float dx = p.x - camera.target.x;
        float dz = p.z - camera.target.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > visibleSq) continue;

        if (prop.rock) {
            DrawSphereEx({p.x, p.y + 0.15f * prop.scale, p.z}, 0.24f * prop.scale, 3, 5, Color{111, 113, 105, 255});
            continue;
        }

        Color trunk{92, 70, 49, 255};
        Color leaves = (prop.seed & 1) ? Color{64, 112, 64, 255} : Color{75, 126, 70, 255};
        if (d2 < detailSq) {
            DrawCylinder({p.x, p.y + 0.34f * prop.scale, p.z}, 0.07f * prop.scale, 0.09f * prop.scale, 0.68f * prop.scale, 5, trunk);
            DrawSphereEx({p.x, p.y + 0.94f * prop.scale, p.z}, 0.43f * prop.scale, 3, 6, leaves);
        } else {
            DrawCube({p.x, p.y + 0.30f * prop.scale, p.z}, 0.10f * prop.scale, 0.60f * prop.scale, 0.10f * prop.scale, trunk);
            DrawSphereEx({p.x, p.y + 0.90f * prop.scale, p.z}, 0.39f * prop.scale, 2, 4, leaves);
        }
    }
}

void City::DrawStreetLight(Vector3 p, bool alongX) const {
    Color pole{87, 91, 91, 255};
    DrawCylinder({p.x, p.y + 0.64f, p.z}, 0.025f, 0.035f, 1.25f, 5, pole);
    Vector3 lampPos = p;
    lampPos.y += 1.25f;
    if (alongX) lampPos.x += 0.14f; else lampPos.z += 0.14f;
    DrawCube(lampPos, 0.22f, 0.08f, 0.12f, Color{221, 213, 173, 255});
}

void City::DrawRoadTile(int x, int z, bool detailed) const {
    Vector3 p = GridToWorld(x, z);
    float y = p.y + 0.025f;
    Color asphalt{62, 66, 68, 255};
    DrawCube({p.x, y, p.z}, CELL_SIZE * 0.98f, 0.10f, CELL_SIZE * 0.98f, asphalt);

    bool n = RoadAt(x, z - 1), s = RoadAt(x, z + 1), e = RoadAt(x + 1, z), w = RoadAt(x - 1, z);
    bool horizontal = e || w;
    bool vertical = n || s;
    Color line{210, 203, 157, 230};

    if (!detailed) {
        if (horizontal && !vertical) DrawCube({p.x, y + 0.06f, p.z}, CELL_SIZE * 0.52f, 0.018f, 0.035f, line);
        else if (vertical && !horizontal) DrawCube({p.x, y + 0.06f, p.z}, 0.035f, 0.018f, CELL_SIZE * 0.52f, line);
        return;
    }

    Color curb{155, 157, 151, 255};
    Color sidewalk{174, 172, 163, 255};
    float sw = 0.20f;
    if (!n) {
        DrawCube({p.x, y + 0.08f, p.z - CELL_SIZE * 0.42f}, CELL_SIZE * 0.96f, 0.08f, sw, sidewalk);
        DrawCube({p.x, y + 0.055f, p.z - CELL_SIZE * 0.33f}, CELL_SIZE * 0.96f, 0.07f, 0.035f, curb);
    }
    if (!s) {
        DrawCube({p.x, y + 0.08f, p.z + CELL_SIZE * 0.42f}, CELL_SIZE * 0.96f, 0.08f, sw, sidewalk);
        DrawCube({p.x, y + 0.055f, p.z + CELL_SIZE * 0.33f}, CELL_SIZE * 0.96f, 0.07f, 0.035f, curb);
    }
    if (!w) {
        DrawCube({p.x - CELL_SIZE * 0.42f, y + 0.08f, p.z}, sw, 0.08f, CELL_SIZE * 0.96f, sidewalk);
        DrawCube({p.x - CELL_SIZE * 0.33f, y + 0.055f, p.z}, 0.035f, 0.07f, CELL_SIZE * 0.96f, curb);
    }
    if (!e) {
        DrawCube({p.x + CELL_SIZE * 0.42f, y + 0.08f, p.z}, sw, 0.08f, CELL_SIZE * 0.96f, sidewalk);
        DrawCube({p.x + CELL_SIZE * 0.33f, y + 0.055f, p.z}, 0.035f, 0.07f, CELL_SIZE * 0.96f, curb);
    }

    if (horizontal && !vertical) {
        DrawCube({p.x - 0.48f, y + 0.06f, p.z}, 0.46f, 0.018f, 0.035f, line);
        DrawCube({p.x + 0.48f, y + 0.06f, p.z}, 0.46f, 0.018f, 0.035f, line);
    } else if (vertical && !horizontal) {
        DrawCube({p.x, y + 0.06f, p.z - 0.48f}, 0.035f, 0.018f, 0.46f, line);
        DrawCube({p.x, y + 0.06f, p.z + 0.48f}, 0.035f, 0.018f, 0.46f, line);
    }

    if (HashCell(x, z, 90) % 23u == 0u && (horizontal != vertical)) {
        if (horizontal && !n) DrawStreetLight({p.x, p.y + 0.08f, p.z - CELL_SIZE * 0.42f}, true);
        else if (vertical && !w) DrawStreetLight({p.x - CELL_SIZE * 0.42f, p.y + 0.08f, p.z}, false);
    }
}

void City::DrawRoads(const Camera3D& camera) const {
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.80f + 32.0f;
    float detailRadius = cameraDistance * 0.50f + 28.0f;
    float visibleSq = visibleRadius * visibleRadius;
    float detailSq = detailRadius * detailRadius;

    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            if (!RoadAt(x, z)) continue;
            Vector3 p = GridToWorld(x, z);
            float dx = p.x - camera.target.x;
            float dz = p.z - camera.target.z;
            float d2 = dx * dx + dz * dz;
            if (d2 > visibleSq) continue;
            DrawRoadTile(x, z, d2 <= detailSq);
        }
    }
}

void City::DrawZones(const Camera3D& camera) const {
    if (tool_ != Tool::Residential && tool_ != Tool::Commercial && tool_ != Tool::Industrial) return;
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.70f + 28.0f;
    float visibleSq = visibleRadius * visibleRadius;

    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            if (zones_[z][x] == Zone::None || RoadAt(x, z) || IsWater(x, z)) continue;
            Vector3 p = GridToWorld(x, z);
            float dx = p.x - camera.target.x;
            float dz = p.z - camera.target.z;
            if (dx * dx + dz * dz > visibleSq) continue;
            if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) continue;
            DrawCube({p.x, p.y + 0.012f, p.z}, CELL_SIZE * 0.82f, 0.025f, CELL_SIZE * 0.82f, ZoneColor(zones_[z][x], 70));
        }
    }
}

void City::DrawBuildings(const Camera3D& camera) const {
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.85f + 35.0f;
    float detailRadius = cameraDistance * 0.43f + 24.0f;
    float visibleSq = visibleRadius * visibleRadius;
    float detailSq = detailRadius * detailRadius;

    for (const auto& b : buildings_) {
        Vector3 center = LotCenter(b.x, b.z, b.w, b.d);
        float dx = center.x - camera.target.x;
        float dz = center.z - camera.target.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > visibleSq) continue;
        BuildingRenderer::DrawBuilding(b, center, b.w * CELL_SIZE, b.d * CELL_SIZE, d2 <= detailSq);
    }
}

void City::DrawServices(const Camera3D& camera) const {
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.85f + 35.0f;
    float visibleSq = visibleRadius * visibleRadius;
    for (const auto& s : services_) {
        Vector3 center = LotCenter(s.x, s.z, s.w, s.d);
        float dx = center.x - camera.target.x;
        float dz = center.z - camera.target.z;
        if (dx * dx + dz * dz > visibleSq) continue;
        BuildingRenderer::DrawService(s, center, s.w * CELL_SIZE, s.d * CELL_SIZE, center.y);
    }
}

void City::DrawPlacementPreview() const {
    if (hoverX_ < 0 || hoverZ_ < 0) return;
    int w = 1, d = 1;
    Color c = tool_ == Tool::Bulldoze ? Color{226, 74, 74, 255} : Color{245, 218, 71, 255};
    if (tool_ == Tool::Power || tool_ == Tool::Park) { w = 2; d = 2; }
    Vector3 p = LotCenter(hoverX_, hoverZ_, w, d);
    DrawCubeWires({p.x, p.y + 0.17f, p.z}, w * CELL_SIZE * 0.94f, 0.27f, d * CELL_SIZE * 0.94f, c);

    if (tool_ == Tool::Power || tool_ == Tool::Water || tool_ == Tool::Park) {
        float radiusCells = tool_ == Tool::Power ? 24.0f : (tool_ == Tool::Water ? 22.0f : 10.0f);
        float radiusWorld = radiusCells * CELL_SIZE;
        DrawCircle3D({p.x, p.y + 0.08f, p.z}, radiusWorld, {1.0f, 0.0f, 0.0f}, 90.0f,
                     tool_ == Tool::Power ? Color{236, 197, 69, 115} : (tool_ == Tool::Water ? Color{62, 151, 226, 115} : Color{91, 190, 101, 115}));
    }
}

void City::Draw3D(const Camera3D& camera) const {
    DrawTerrain();
    DrawNaturalProps(camera);
    DrawZones(camera);
    DrawRoads(camera);
    DrawServices(camera);
    DrawBuildings(camera);
    DrawPlacementPreview();
}

void City::DrawBar(int x, int y, int w, float value, Color color) const {
    value = Clamp(value, 0.0f, 1.0f);
    DrawRectangle(x, y, w, 11, Color{45, 49, 51, 255});
    DrawRectangle(x, y, (int)(w * value), 11, color);
}

const char* City::ToolLabel(Tool tool) const {
    switch (tool) {
        case Tool::Road: return "Strasse";
        case Tool::Residential: return "Wohnen";
        case Tool::Commercial: return "Gewerbe";
        case Tool::Industrial: return "Industrie";
        case Tool::Bulldoze: return "Bulldozer";
        case Tool::Power: return "Kraftwerk";
        case Tool::Water: return "Wasserturm";
        case Tool::Park: return "Park";
        default: return "";
    }
}

void City::DrawTopBar() const {
    int w = GetScreenWidth();
    DrawRectangle(0, 0, w, 64, Color{21, 27, 30, 247});
    DrawText("CITY LAB v0.4.1", 18, 15, 25, RAYWHITE);

    std::string money = "CHF " + std::to_string(money_);
    std::string pop = "Einwohner " + std::to_string(population_);
    std::string jobs = "Jobs " + std::to_string(filledJobs_) + "/" + std::to_string(totalJobs_);
    std::string date = std::to_string(day_) + "." + std::to_string(month_) + "." + std::to_string(year_);
    std::string fps = "FPS " + std::to_string(GetFPS());
    DrawText(money.c_str(), 202, 19, 17, Color{233, 235, 232, 255});
    DrawText(pop.c_str(), 338, 19, 17, Color{233, 235, 232, 255});
    DrawText(jobs.c_str(), 505, 19, 17, Color{233, 235, 232, 255});
    DrawText(date.c_str(), 670, 19, 16, Color{198, 204, 203, 255});
    DrawText(fps.c_str(), 785, 19, 14, GetFPS() >= 45 ? Color{116, 205, 137, 255} : Color{229, 139, 93, 255});

    const char* speedLabels[4] = {"||", "1x", "2x", "3x"};
    for (int i = 0; i < 4; ++i) {
        Rectangle r{(float)w - 264 + i * 50.0f, 16, 44, 30};
        Color bg = gameSpeed_ == i ? Color{77, 105, 93, 255} : Color{44, 50, 52, 255};
        DrawRectangleRounded(r, 0.20f, 6, bg);
        DrawText(speedLabels[i], (int)r.x + 12, (int)r.y + 7, 15, RAYWHITE);
    }
}

void City::DrawToolPanel() const {
    Rectangle panel{14, 82, 238, 355};
    DrawRectangleRounded(panel, 0.055f, 8, Color{20, 25, 27, 238});
    DrawText("BAUEN", 28, 97, 19, RAYWHITE);

    const char* labels[8] = {
        "1  Strasse   CHF 120",
        "2  Wohnen",
        "3  Gewerbe",
        "4  Industrie",
        "5  Bulldozer",
        "6  Kraftwerk   CHF 6500",
        "7  Wasserturm  CHF 3600",
        "8  Park        CHF 1200"
    };
    for (int i = 0; i < 8; ++i) {
        Rectangle r{28.0f, 130.0f + i * 37.0f, 210.0f, 31.0f};
        Color bg = (int)tool_ == i ? Color{69, 91, 82, 255} : Color{38, 44, 46, 255};
        DrawRectangleRounded(r, 0.13f, 6, bg);
        DrawText(labels[i], 38, (int)r.y + 8, 13, RAYWHITE);
    }
    DrawText("WASD bewegen | Rechtsziehen drehen", 28, 410, 12, Color{184, 190, 190, 255});
    DrawText("Mausrad zoom | SPACE Pause", 28, 426, 12, Color{184, 190, 190, 255});
}

void City::DrawDemandPanel() const {
    int w = GetScreenWidth();
    Rectangle panel{(float)w - 262, 82, 244, 176};
    DrawRectangleRounded(panel, 0.055f, 8, Color{20, 25, 27, 238});
    DrawText("NACHFRAGE", w - 244, 98, 18, RAYWHITE);

    DrawText("Wohnen", w - 244, 132, 14, Color{213, 216, 214, 255});
    DrawBar(w - 151, 134, 116, residentialDemand_, Color{75, 191, 107, 255});
    DrawText("Gewerbe", w - 244, 162, 14, Color{213, 216, 214, 255});
    DrawBar(w - 151, 164, 116, commercialDemand_, Color{69, 147, 232, 255});
    DrawText("Industrie", w - 244, 192, 14, Color{213, 216, 214, 255});
    DrawBar(w - 151, 194, 116, industrialDemand_, Color{227, 178, 61, 255});

    std::string happy = "Zufriedenheit " + std::to_string((int)(happiness_ * 100.0f)) + "%";
    DrawText(happy.c_str(), w - 244, 226, 13, Color{203, 209, 206, 255});
}

void City::DrawInfoPanel() const {
    int w = GetScreenWidth();
    Rectangle panel{(float)w - 262, 270, 244, 194};
    DrawRectangleRounded(panel, 0.055f, 8, Color{20, 25, 27, 238});
    DrawText("STADT", w - 244, 286, 18, RAYWHITE);

    std::string power = "Strom " + std::to_string(powerUsed_) + " / " + std::to_string(powerCapacity_);
    std::string water = "Wasser " + std::to_string(waterUsed_) + " / " + std::to_string(waterCapacity_);
    std::string unemployment = "Arbeitslos " + std::to_string((int)(unemployment_ * 100.0f)) + "%";
    std::string income = "Einnahmen  +" + std::to_string(projectedIncome_);
    std::string costs = "Ausgaben   -" + std::to_string(projectedExpenses_);
    std::string net = "Monat      " + std::string(projectedIncome_ - projectedExpenses_ >= 0 ? "+" : "") + std::to_string(projectedIncome_ - projectedExpenses_);

    DrawText(power.c_str(), w - 244, 322, 14, powerCapacity_ > 0 && powerUsed_ <= powerCapacity_ ? Color{225, 205, 92, 255} : Color{228, 105, 78, 255});
    DrawText(water.c_str(), w - 244, 346, 14, waterCapacity_ > 0 && waterUsed_ <= waterCapacity_ ? Color{93, 169, 231, 255} : Color{228, 105, 78, 255});
    DrawText(unemployment.c_str(), w - 244, 370, 14, Color{205, 210, 208, 255});
    DrawText(income.c_str(), w - 244, 398, 14, Color{105, 201, 125, 255});
    DrawText(costs.c_str(), w - 244, 420, 14, Color{224, 131, 105, 255});
    DrawText(net.c_str(), w - 244, 442, 14, projectedIncome_ >= projectedExpenses_ ? Color{111, 207, 132, 255} : Color{229, 103, 87, 255});

    if (unpoweredBuildings_ > 0 || unwateredBuildings_ > 0) {
        std::string warn = "Probleme: ";
        if (unpoweredBuildings_ > 0) warn += std::to_string(unpoweredBuildings_) + " ohne Strom ";
        if (unwateredBuildings_ > 0) warn += std::to_string(unwateredBuildings_) + " ohne Wasser";
        DrawText(warn.c_str(), 270, GetScreenHeight() - 31, 14, Color{236, 183, 74, 255});
    } else if (services_.empty()) {
        DrawText("Tipp: Baue zuerst Kraftwerk + Wasserturm, dann zonieren.", 270, GetScreenHeight() - 31, 14, Color{236, 210, 116, 255});
    } else {
        DrawText("Gebaeude wachsen bei Nachfrage, Versorgung und hoher Attraktivitaet.", 270, GetScreenHeight() - 31, 14, Color{214, 219, 216, 255});
    }
}

void City::DrawUI() const {
    DrawTopBar();
    DrawToolPanel();
    DrawDemandPanel();
    DrawInfoPanel();

    std::string selected = std::string("Ausgewaehlt: ") + ToolLabel(tool_);
    DrawText(selected.c_str(), 28, GetScreenHeight() - 31, 14, Color{224, 228, 225, 255});
}
