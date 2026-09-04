#include "City.h"
#include "BuildingRenderer.h"
#include "raymath.h"
#include "rlgl.h"
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

constexpr int ROAD_DIRS[8][2] = {
    {0,-1},{1,-1},{1,0},{1,1},
    {0,1},{-1,1},{-1,0},{-1,-1}
};

int RoadDirIndex(int dx, int dz) {
    for (int i = 0; i < 8; ++i) {
        if (ROAD_DIRS[i][0] == dx && ROAD_DIRS[i][1] == dz) return i;
    }
    return -1;
}

int OppositeRoadDir(int dir) {
    return (dir + 4) & 7;
}
}

City::City() {
    BuildTerrainCache();
    GenerateNaturalProps();
    BuildTerrainModels();

    // Small starter road so the player immediately has a visible anchor.
    int previousX = -1;
    RoadVisualPath starterVisual;
    for (int x = 38; x <= 53; ++x) {
        if (!IsWater(x, 48)) {
            roads_[48][x] = true;
            ++roadCount_;
            if (previousX >= 0) LinkRoadCells(previousX, 48, x, 48);
            previousX = x;

            Vector3 p = GridToWorld(x, 48);
            p.y = TerrainHeightAtWorld(p.x, p.z);
            starterVisual.points.push_back(p);
        } else {
            if (starterVisual.points.size() >= 2) {
                roadVisualPaths_.push_back(starterVisual);
            }
            starterVisual.points.clear();
            previousX = -1;
        }
    }
    if (starterVisual.points.size() >= 2) roadVisualPaths_.push_back(starterVisual);
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

bool City::MouseToWorld(const Camera3D& camera, Vector3& hit) const {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.00001f) return false;

    float targetY = 0.65f;
    float t = (targetY - ray.position.y) / ray.direction.y;
    if (t < 0.0f) return false;

    for (int i = 0; i < 5; ++i) {
        hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        float h = TerrainHeightAtWorld(hit.x, hit.z);
        t = (h - ray.position.y) / ray.direction.y;
        if (t < 0.0f) return false;
    }

    hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    float gx = hit.x / CELL_SIZE + GRID_W / 2.0f;
    float gz = hit.z / CELL_SIZE + GRID_H / 2.0f;
    return gx >= 0.0f && gx < GRID_W && gz >= 0.0f && gz < GRID_H;
}

bool City::MouseToGrid(const Camera3D& camera, int& gx, int& gz) const {
    Vector3 hit{};
    if (!MouseToWorld(camera, hit)) return false;

    gx = (int)floorf(hit.x / CELL_SIZE + GRID_W / 2.0f);
    gz = (int)floorf(hit.z / CELL_SIZE + GRID_H / 2.0f);
    return Inside(gx, gz);
}

bool City::RoadAt(int x, int z) const {
    return Inside(x, z) && roads_[z][x];
}

float City::DistanceToVisibleRoad(float wx, float wz, Vector3* nearest, Vector3* tangent) const {
    float bestSq = 1.0e30f;
    Vector3 bestPoint{wx, TerrainHeightAtWorld(wx, wz), wz};
    Vector3 bestTangent{1.0f, 0.0f, 0.0f};

    for (const auto& path : roadVisualPaths_) {
        if (path.points.size() < 2) continue;

        for (size_t i = 0; i + 1 < path.points.size(); ++i) {
            const Vector3& a = path.points[i];
            const Vector3& b = path.points[i + 1];

            float abx = b.x - a.x;
            float abz = b.z - a.z;
            float lenSq = abx * abx + abz * abz;
            if (lenSq < 0.00001f) continue;

            float apx = wx - a.x;
            float apz = wz - a.z;
            float t = Clamp((apx * abx + apz * abz) / lenSq, 0.0f, 1.0f);

            float px = a.x + abx * t;
            float pz = a.z + abz * t;
            float dx = wx - px;
            float dz = wz - pz;
            float d2 = dx * dx + dz * dz;

            if (d2 < bestSq) {
                bestSq = d2;
                bestPoint = {px, TerrainHeightAtWorld(px, pz), pz};

                float len = sqrtf(lenSq);
                bestTangent = {abx / len, 0.0f, abz / len};
            }
        }
    }

    if (nearest) *nearest = bestPoint;
    if (tangent) *tangent = bestTangent;
    return sqrtf(bestSq);
}

bool City::CellOverlapsVisibleRoad(int x, int z, float extraMargin) const {
    if (!Inside(x, z)) return false;
    Vector3 p = GridToWorld(x, z);

    // Zone tile half-width + sidewalk half-width. If the cell centre is closer
    // than this, the visible square would overlap the actual curved road.
    float exclusion = CELL_SIZE * 0.80f + extraMargin;
    return DistanceToVisibleRoad(p.x, p.z) < exclusion;
}

bool City::ZoneCellEligible(int x, int z) const {
    if (!Inside(x, z) || IsWater(x, z) || RoadAt(x, z)) return false;
    if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) return false;

    Vector3 p = GridToWorld(x, z);
    float d = DistanceToVisibleRoad(p.x, p.z);

    // Roughly two rows of zoning around a road for the current lot system.
    return d >= CELL_SIZE * 0.80f && d <= CELL_SIZE * 2.65f;
}

bool City::RoadConnected(int x, int z, int nx, int nz) const {
    if (!RoadAt(x, z) || !RoadAt(nx, nz)) return false;
    int dir = RoadDirIndex(nx - x, nz - z);
    if (dir < 0) return false;
    return (roadLinks_[z][x] & (1u << dir)) != 0;
}

void City::LinkRoadCells(int x, int z, int nx, int nz) {
    if (!Inside(x, z) || !Inside(nx, nz)) return;
    int dir = RoadDirIndex(nx - x, nz - z);
    if (dir < 0) return;
    roads_[z][x] = true;
    roads_[nz][nx] = true;
    roadLinks_[z][x] |= (unsigned char)(1u << dir);
    roadLinks_[nz][nx] |= (unsigned char)(1u << OppositeRoadDir(dir));
}

void City::UnlinkRoadCell(int x, int z) {
    if (!Inside(x, z)) return;
    unsigned char links = roadLinks_[z][x];
    for (int dir = 0; dir < 8; ++dir) {
        if ((links & (1u << dir)) == 0) continue;
        int nx = x + ROAD_DIRS[dir][0];
        int nz = z + ROAD_DIRS[dir][1];
        if (!Inside(nx, nz)) continue;
        roadLinks_[nz][nx] &= (unsigned char)~(1u << OppositeRoadDir(dir));
    }
    roadLinks_[z][x] = 0;
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
            if (CellOverlapsVisibleRoad(xx, zz, CELL_SIZE * 0.05f)) return false;
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
            if (CellOverlapsVisibleRoad(xx, zz, CELL_SIZE * 0.08f)) return false;
            float h = TerrainHeight(xx, zz);
            minH = std::min(minH, h);
            maxH = std::max(maxH, h);
        }
    }
    return (maxH - minH) < 0.62f;
}

bool City::LotAdjacentToRoad(int x, int z, int w, int d) const {
    if (!Inside(x, z) || !Inside(x + w - 1, z + d - 1)) return false;

    float minDistance = 1.0e30f;
    for (int zz = z; zz < z + d; ++zz) {
        for (int xx = x; xx < x + w; ++xx) {
            Vector3 p = GridToWorld(xx, zz);
            minDistance = std::min(minDistance, DistanceToVisibleRoad(p.x, p.z));
        }
    }

    // The lot must touch the road's zoning band, but may never overlap it.
    return minDistance >= CELL_SIZE * 0.76f &&
           minDistance <= CELL_SIZE * 1.55f;
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
    roadDragWorld_.clear();
    roadDragCurrentValid_ = false;
}

void City::SelectToolExternal(int tool) {
    if (tool < 0 || tool > 7) return;
    SetTool((Tool)tool);
}

void City::SetGameSpeedExternal(int speed) {
    gameSpeed_ = std::max(0, std::min(3, speed));
}

void City::Update(float dt, const Camera3D& camera) {
    HandleInput(camera);
    Simulate(dt);
    UpdateTraffic(dt);
}

void City::HandleUIInput() {
#if defined(PLATFORM_WEB)
    // Web uses the HTML/CSS HUD in shell.html.
    return;
#else
    Vector2 m = GetMousePosition();
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const float hudTop = (float)sh - 136.0f;

    Rectangle speedRects[4] = {
        {(float)sw - 206.0f, hudTop + 10.0f, 40.0f, 32.0f},
        {(float)sw - 160.0f, hudTop + 10.0f, 40.0f, 32.0f},
        {(float)sw - 114.0f, hudTop + 10.0f, 40.0f, 32.0f},
        {(float)sw - 68.0f,  hudTop + 10.0f, 40.0f, 32.0f}
    };
    for (int i = 0; i < 4; ++i) {
        if (CheckCollisionPointRec(m, speedRects[i])) {
            gameSpeed_ = i;
            return;
        }
    }
#endif
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

    Vector3 worldHit{};
    bool hasWorldHit = MouseToWorld(camera, worldHit);

    int gx = -1, gz = -1;
    if (hasWorldHit) {
        gx = (int)floorf(worldHit.x / CELL_SIZE + GRID_W / 2.0f);
        gz = (int)floorf(worldHit.z / CELL_SIZE + GRID_H / 2.0f);
    }

    if (Inside(gx, gz)) {
        hoverX_ = gx;
        hoverZ_ = gz;
    } else {
        hoverX_ = hoverZ_ = -1;
    }

    Vector2 m = GetMousePosition();
#if defined(PLATFORM_WEB)
    if (m.y < 8.0f) return;
#else
    const float hudTop = (float)GetScreenHeight() - 136.0f;
    if (m.y < 42.0f || m.y > hudTop) return;
#endif

    if (tool_ == Tool::Road && dragging_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hasWorldHit) {
        roadDragCurrent_ = worldHit;
        roadDragCurrentValid_ = true;

        if (roadDragWorld_.empty()) {
            roadDragWorld_.push_back(worldHit);
        } else {
            Vector3 last = roadDragWorld_.back();
            float dx = worldHit.x - last.x;
            float dz = worldHit.z - last.z;
            if (dx * dx + dz * dz >= (CELL_SIZE * 0.42f) * (CELL_SIZE * 0.42f)) {
                roadDragWorld_.push_back(worldHit);
            }
        }
    }

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

            if (tool_ == Tool::Road) {
                roadDragWorld_.clear();
                Vector3 start = GridToWorld(hoverX_, hoverZ_);
                start.y = TerrainHeightAtWorld(start.x, start.z);
                roadDragWorld_.push_back(start);
                roadDragCurrent_ = hasWorldHit ? worldHit : start;
                roadDragCurrentValid_ = true;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && dragging_) {
        if (tool_ == Tool::Road) {
            if (hasWorldHit) {
                roadDragCurrent_ = worldHit;
                roadDragCurrentValid_ = true;
            }
            PlaceDraggedRoad();
        } else if (hoverX_ >= 0) {
            if (tool_ == Tool::Residential) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Residential);
            else if (tool_ == Tool::Commercial) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Commercial);
            else if (tool_ == Tool::Industrial) PaintZoneRect(dragStartX_, dragStartZ_, hoverX_, hoverZ_, Zone::Industrial);
        }

        dragging_ = false;
        roadDragWorld_.clear();
        roadDragCurrentValid_ = false;
    }
}

void City::PlaceRoadLine(int x0, int z0, int x1, int z1) {
    roadDragWorld_.clear();
    roadDragWorld_.push_back(GridToWorld(x0, z0));
    roadDragWorld_.push_back(GridToWorld(x1, z1));
    roadDragCurrent_ = GridToWorld(x1, z1);
    roadDragCurrentValid_ = true;
    PlaceDraggedRoad();
    roadDragWorld_.clear();
    roadDragCurrentValid_ = false;
}

std::vector<Vector3> City::BuildDraggedRoadCurve() const {
    std::vector<Vector3> gesture = roadDragWorld_;

    if (roadDragCurrentValid_) {
        if (gesture.empty()) {
            gesture.push_back(roadDragCurrent_);
        } else {
            Vector3 last = gesture.back();
            float dx = roadDragCurrent_.x - last.x;
            float dz = roadDragCurrent_.z - last.z;
            if (dx * dx + dz * dz > 0.01f) gesture.push_back(roadDragCurrent_);
        }
    }

    if (gesture.size() < 2) return gesture;

    auto snapToVisibleRoad = [&](Vector3 input, float radius, Vector3& output) {
        float bestSq = radius * radius;
        bool found = false;

        for (const auto& path : roadVisualPaths_) {
            if (path.points.size() < 2) continue;

            for (size_t i = 0; i + 1 < path.points.size(); ++i) {
                const Vector3& a = path.points[i];
                const Vector3& b = path.points[i + 1];

                float abx = b.x - a.x;
                float abz = b.z - a.z;
                float lenSq = abx * abx + abz * abz;
                if (lenSq < 0.00001f) continue;

                float apx = input.x - a.x;
                float apz = input.z - a.z;
                float t = Clamp((apx * abx + apz * abz) / lenSq, 0.0f, 1.0f);

                Vector3 p{
                    a.x + abx * t,
                    0.0f,
                    a.z + abz * t
                };
                p.y = TerrainHeightAtWorld(p.x, p.z);

                float dx = p.x - input.x;
                float dz = p.z - input.z;
                float d2 = dx * dx + dz * dz;

                if (d2 < bestSq) {
                    bestSq = d2;
                    output = p;
                    found = true;
                }
            }
        }
        return found;
    };

    Vector3 start = gesture.front();
    Vector3 end = gesture.back();

    // Snap to the visible centreline, not just the hidden simulation grid.
    Vector3 snapped{};
    if (snapToVisibleRoad(start, CELL_SIZE * 0.78f, snapped)) start = snapped;
    if (snapToVisibleRoad(end, CELL_SIZE * 0.88f, snapped)) end = snapped;

    start.y = TerrainHeightAtWorld(start.x, start.z);
    end.y = TerrainHeightAtWorld(end.x, end.z);

    float chordX = end.x - start.x;
    float chordZ = end.z - start.z;
    float chordLength = sqrtf(chordX * chordX + chordZ * chordZ);

    if (chordLength < CELL_SIZE * 0.55f) return {start, end};

    float ux = chordX / chordLength;
    float uz = chordZ / chordLength;
    float nx = -uz;
    float nz = ux;

    float strongest = 0.0f;
    for (size_t i = 1; i + 1 < gesture.size(); ++i) {
        const Vector3& p = gesture[i];
        float vx = p.x - start.x;
        float vz = p.z - start.z;

        float along = (vx * ux + vz * uz) / chordLength;
        if (along <= 0.05f || along >= 0.95f) continue;

        float signedSide = vx * nx + vz * nz;
        float centreWeight = sinf(PI * Clamp(along, 0.0f, 1.0f));
        float candidate = signedSide * centreWeight;

        if (fabsf(candidate) > fabsf(strongest)) strongest = candidate;
    }

    if (fabsf(strongest) < CELL_SIZE * 0.22f) {
        int steps = std::max(2, (int)ceilf(chordLength / (CELL_SIZE * 0.18f)));
        std::vector<Vector3> straight;
        straight.reserve((size_t)steps + 1);

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / steps;
            Vector3 p{
                start.x + chordX * t,
                0.0f,
                start.z + chordZ * t
            };
            p.y = TerrainHeightAtWorld(p.x, p.z);
            straight.push_back(p);
        }
        return straight;
    }

    float sign = strongest >= 0.0f ? 1.0f : -1.0f;
    float desiredSagitta = fabsf(strongest);

    const float minimumRadius = CELL_SIZE * 2.75f;
    float maxSagittaForRadius = chordLength * 0.34f;

    if (chordLength < minimumRadius * 2.0f) {
        float inside = minimumRadius * minimumRadius - chordLength * chordLength * 0.25f;
        if (inside > 0.0f) {
            float radiusLimited = minimumRadius - sqrtf(inside);
            maxSagittaForRadius = std::min(maxSagittaForRadius, radiusLimited);
        }
    }

    float sagitta = std::min(desiredSagitta, maxSagittaForRadius);
    sagitta = std::max(sagitta, CELL_SIZE * 0.15f);

    float radius = chordLength * chordLength / (8.0f * sagitta) + sagitta * 0.5f;
    float centreDistance = radius - sagitta;

    Vector3 mid{
        (start.x + end.x) * 0.5f,
        0.0f,
        (start.z + end.z) * 0.5f
    };

    int steps = std::max(12, (int)ceilf(chordLength / (CELL_SIZE * 0.14f)));
    std::vector<Vector3> arc;
    arc.reserve((size_t)steps + 1);

    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps;
        float x = (t - 0.5f) * chordLength;
        float underRoot = std::max(0.0f, radius * radius - x * x);
        float offset = sign * (sqrtf(underRoot) - centreDistance);

        Vector3 p{
            mid.x + ux * x + nx * offset,
            0.0f,
            mid.z + uz * x + nz * offset
        };
        p.y = TerrainHeightAtWorld(p.x, p.z);
        arc.push_back(p);
    }

    arc.front() = start;
    arc.back() = end;
    return arc;
}

bool City::RoadCurvePointValid(const Vector3& p) const {
    int x = (int)floorf(p.x / CELL_SIZE + GRID_W / 2.0f);
    int z = (int)floorf(p.z / CELL_SIZE + GRID_H / 2.0f);
    if (!Inside(x, z) || IsWater(x, z)) return false;
    if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) return false;
    return true;
}

void City::PlaceDraggedRoad() {
    std::vector<Vector3> curve = BuildDraggedRoadCurve();
    if (curve.size() < 2) return;

    int prevX = -1;
    int prevZ = -1;
    bool havePrev = false;
    RoadVisualPath visual;
    visual.points.reserve(curve.size());

    auto addCell = [&](int x, int z) -> bool {
        if (!Inside(x, z) || IsWater(x, z)) return false;
        if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) return false;

        if (!RoadAt(x, z)) {
            if (money_ < 120) return false;
            roads_[z][x] = true;
            roadLinks_[z][x] = 0;
            zones_[z][x] = Zone::None;
            money_ -= 120;
            ++roadCount_;
        }

        if (havePrev && (x != prevX || z != prevZ)) {
            int dx = x - prevX;
            int dz = z - prevZ;

            while (abs(dx) > 1 || abs(dz) > 1) {
                int sx = prevX + (dx > 0 ? 1 : (dx < 0 ? -1 : 0));
                int sz = prevZ + (dz > 0 ? 1 : (dz < 0 ? -1 : 0));

                if (!Inside(sx, sz) || IsWater(sx, sz)) return false;
                if (BuildingIndexAt(sx, sz) >= 0 || ServiceIndexAt(sx, sz) >= 0) return false;

                if (!RoadAt(sx, sz)) {
                    if (money_ < 120) return false;
                    roads_[sz][sx] = true;
                    roadLinks_[sz][sx] = 0;
                    zones_[sz][sx] = Zone::None;
                    money_ -= 120;
                    ++roadCount_;
                }

                LinkRoadCells(prevX, prevZ, sx, sz);
                prevX = sx;
                prevZ = sz;
                dx = x - prevX;
                dz = z - prevZ;
            }

            if (x != prevX || z != prevZ) LinkRoadCells(prevX, prevZ, x, z);
        }

        prevX = x;
        prevZ = z;
        havePrev = true;
        return true;
    };

    for (const auto& raw : curve) {
        Vector3 p = raw;
        p.y = TerrainHeightAtWorld(p.x, p.z);

        int x = (int)floorf(p.x / CELL_SIZE + GRID_W / 2.0f);
        int z = (int)floorf(p.z / CELL_SIZE + GRID_H / 2.0f);

        if (!Inside(x, z) || IsWater(x, z)) break;
        if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) break;

        if (!havePrev || x != prevX || z != prevZ) {
            if (!addCell(x, z)) break;
        }

        visual.points.push_back(p);
    }

    // Keep the original smoothed mouse gesture for rendering. The old graph
    // remains underneath for zoning, services and traffic.
    if (visual.points.size() >= 2) {
        roadVisualPaths_.push_back(std::move(visual));
    }

    vehicles_.clear();
    RecalculateBudgetPreview();
}

void City::PaintZoneRect(int x0, int z0, int x1, int z1, Zone zone) {
    int xa = std::min(x0, x1), xb = std::max(x0, x1);
    int za = std::min(z0, z1), zb = std::max(z0, z1);

    for (int z = za; z <= zb; ++z) {
        for (int x = xa; x <= xb; ++x) {
            if (!ZoneCellEligible(x, z)) continue;
            zones_[z][x] = zone;
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
            UnlinkRoadCell(x, z);
            roads_[z][x] = false;
            roadCount_ = std::max(0, roadCount_ - 1);
            vehicles_.clear();
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

void City::SpawnVehicle() {
    if (roadCount_ < 2) return;

    static const Color carColors[] = {
        {195, 67, 62, 255}, {63, 112, 165, 255}, {220, 205, 177, 255},
        {55, 61, 64, 255}, {204, 145, 55, 255}, {89, 139, 104, 255},
        {165, 168, 171, 255}, {125, 86, 145, 255}
    };

    for (int attempt = 0; attempt < 90; ++attempt) {
        int x = GetRandomValue(0, GRID_W - 1);
        int z = GetRandomValue(0, GRID_H - 1);
        if (!RoadAt(x, z)) continue;

        Vehicle v;
        v.x = x;
        v.z = z;
        v.speed = 0.58f + GetRandomValue(0, 42) / 100.0f;
        v.color = carColors[GetRandomValue(0, 7)];

        if (!PickNextRoadCell(v)) continue;
        vehicles_.push_back(v);
        return;
    }
}

bool City::PickNextRoadCell(Vehicle& v) {
    int candidates[8][2];
    int count = 0;

    for (int dir = 0; dir < 8; ++dir) {
        if ((roadLinks_[v.z][v.x] & (1u << dir)) == 0) continue;
        int nx = v.x + ROAD_DIRS[dir][0];
        int nz = v.z + ROAD_DIRS[dir][1];

        if (nx == v.prevX && nz == v.prevZ) continue;
        candidates[count][0] = nx;
        candidates[count][1] = nz;
        ++count;
    }

    if (count == 0 && v.prevX >= 0 && RoadConnected(v.x, v.z, v.prevX, v.prevZ)) {
        v.nextX = v.prevX;
        v.nextZ = v.prevZ;
        return true;
    }
    if (count == 0) return false;

    int pick = GetRandomValue(0, count - 1);
    v.nextX = candidates[pick][0];
    v.nextZ = candidates[pick][1];
    return true;
}

void City::UpdateTraffic(float dt) {
    float multiplier = gameSpeed_ == 0 ? 0.0f : (gameSpeed_ == 1 ? 1.0f : (gameSpeed_ == 2 ? 1.65f : 2.30f));
    if (multiplier <= 0.0f) return;

    int desired = std::min(52, std::max(0, roadCount_ / 3 + population_ / 42));
    trafficSpawnTimer_ += dt * multiplier;

    while ((int)vehicles_.size() < desired && trafficSpawnTimer_ > 0.22f) {
        trafficSpawnTimer_ -= 0.22f;
        SpawnVehicle();
    }

    if ((int)vehicles_.size() > desired + 8) {
        vehicles_.resize((size_t)(desired + 8));
    }

    for (size_t i = 0; i < vehicles_.size();) {
        Vehicle& v = vehicles_[i];

        if (!RoadAt(v.x, v.z) || !RoadAt(v.nextX, v.nextZ)) {
            vehicles_.erase(vehicles_.begin() + (long)i);
            continue;
        }

        v.t += dt * multiplier * v.speed;
        while (v.t >= 1.0f) {
            v.t -= 1.0f;
            v.prevX = v.x;
            v.prevZ = v.z;
            v.x = v.nextX;
            v.z = v.nextZ;
            ++completedTrips_;

            if (!PickNextRoadCell(v)) {
                vehicles_.erase(vehicles_.begin() + (long)i);
                goto next_vehicle;
            }
        }

        ++i;
        next_vehicle:;
    }
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
    if (milestoneBannerDays_ > 0) --milestoneBannerDays_;
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
    CheckMilestones();

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
    financialWarning_ = money_ < 0;
}

void City::CheckMilestones() {
    while (population_ >= milestoneTarget_ && cityLevel_ < 6) {
        ++cityLevel_;
        lastMilestoneReward_ = 4000 + cityLevel_ * 2500;
        money_ += lastMilestoneReward_;
        maxBuildingLevel_ = std::min(3, cityLevel_ + 1);
        milestoneBannerDays_ = 12;

        if (cityLevel_ == 2) milestoneTarget_ = 350;
        else if (cityLevel_ == 3) milestoneTarget_ = 800;
        else if (cityLevel_ == 4) milestoneTarget_ = 1500;
        else if (cityLevel_ == 5) milestoneTarget_ = 3000;
        else milestoneTarget_ = 999999;
    }
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

        Vector3 lotCenter = LotCenter(ax, az, w, d);
        Vector3 nearestRoad{};
        Vector3 roadTangent{};
        DistanceToVisibleRoad(lotCenter.x, lotCenter.z, &nearestRoad, &roadTangent);
        Vector3 front{
            nearestRoad.x - lotCenter.x,
            0.0f,
            nearestRoad.z - lotCenter.z
        };
        float frontLen = sqrtf(front.x * front.x + front.z * front.z);
        if (frontLen > 0.001f) {
            front.x /= frontLen;
            front.z /= frontLen;
            b.rotationDegrees = atan2f(front.x, front.z) * RAD2DEG;
        }

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

        if (b.level < std::min(3, maxBuildingLevel_) && b.ageDays > 18 && fullness > 0.72f && b.landValue > (b.level == 1 ? 0.48f : 0.64f)) {
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

    float utilityCoverage = buildings_.empty() ? 0.55f :
        1.0f - (float)(unpoweredBuildings_ + unwateredBuildings_) / std::max(1.0f, (float)buildings_.size() * 2.0f);
    float economy = projectedIncome_ >= projectedExpenses_ ? 1.0f : 0.45f;
    float score = happiness_ * 0.44f + (1.0f - unemployment_) * 0.20f
                + Clamp(utilityCoverage, 0.0f, 1.0f) * 0.26f + economy * 0.10f;
    cityScore_ = (int)roundf(Clamp(score, 0.0f, 1.0f) * 100.0f);
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

Model City::BuildCellSurfaceModel(bool waterSurface) const {
    Mesh mesh{};

    if (waterSurface) {
        mesh.triangleCount = 2;
        mesh.vertexCount = 6;
        mesh.vertices = (float*)MemAlloc((size_t)mesh.vertexCount * 3 * sizeof(float));
        mesh.colors = (unsigned char*)MemAlloc((size_t)mesh.vertexCount * 4 * sizeof(unsigned char));

        const float halfW = GRID_W * CELL_SIZE * 0.5f;
        const float halfH = GRID_H * CELL_SIZE * 0.5f;
        const float y = WATER_LEVEL - 0.020f;
        Vector3 v[6] = {
            {-halfW,y,-halfH}, { halfW,y, halfH}, { halfW,y,-halfH},
            {-halfW,y,-halfH}, {-halfW,y, halfH}, { halfW,y, halfH}
        };
        Color water{48, 121, 164, 242};
        for (int i = 0; i < 6; ++i) {
            mesh.vertices[i*3] = v[i].x;
            mesh.vertices[i*3+1] = v[i].y;
            mesh.vertices[i*3+2] = v[i].z;
            mesh.colors[i*4] = water.r;
            mesh.colors[i*4+1] = water.g;
            mesh.colors[i*4+2] = water.b;
            mesh.colors[i*4+3] = water.a;
        }

        UploadMesh(&mesh, false);
        return LoadModelFromMesh(mesh);
    }

    const int cellCount = GRID_W * GRID_H;
    mesh.triangleCount = cellCount * 2;
    mesh.vertexCount = cellCount * 6;
    mesh.vertices = (float*)MemAlloc((size_t)mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = (unsigned char*)MemAlloc((size_t)mesh.vertexCount * 4 * sizeof(unsigned char));

    int vi = 0;
    auto colorAt = [&](float y, int cx, int cz) {
        float n = Hash01(cx, cz, 177) - 0.5f;
        if (y < WATER_LEVEL + 0.12f) {
            return Color{(unsigned char)Clamp(166.0f + n*8.0f,0.0f,255.0f),
                         (unsigned char)Clamp(157.0f + n*7.0f,0.0f,255.0f),
                         (unsigned char)Clamp(112.0f + n*5.0f,0.0f,255.0f),255};
        }
        if (y > 1.88f) {
            return Color{(unsigned char)Clamp(102.0f+n*8.0f,0.0f,255.0f),
                         (unsigned char)Clamp(119.0f+n*8.0f,0.0f,255.0f),
                         (unsigned char)Clamp(94.0f+n*6.0f,0.0f,255.0f),255};
        }
        float shade = Clamp((y - WATER_LEVEL) / 1.8f, 0.0f, 1.0f);
        return Color{
            (unsigned char)Clamp(73.0f + shade*16.0f + n*5.0f,0.0f,255.0f),
            (unsigned char)Clamp(127.0f + shade*24.0f + n*5.0f,0.0f,255.0f),
            (unsigned char)Clamp(76.0f + shade*13.0f + n*4.0f,0.0f,255.0f),255};
    };
    auto push = [&](float x,float y,float z,Color col) {
        mesh.vertices[vi*3]=x; mesh.vertices[vi*3+1]=y; mesh.vertices[vi*3+2]=z;
        mesh.colors[vi*4]=col.r; mesh.colors[vi*4+1]=col.g;
        mesh.colors[vi*4+2]=col.b; mesh.colors[vi*4+3]=col.a;
        ++vi;
    };
    auto cornerHeight = [&](int cx, int cz) {
        float sum=0.0f; int count=0;
        for(int dz=-1; dz<=0; ++dz) for(int dx=-1; dx<=0; ++dx) {
            int sx=cx+dx, sz=cz+dz;
            if(Inside(sx,sz)){ sum+=TerrainHeight(sx,sz); ++count; }
        }
        return count ? sum/count : 0.0f;
    };

    for (int z=0; z<GRID_H; ++z) {
        for (int x=0; x<GRID_W; ++x) {
            float x0=(x-GRID_W/2)*CELL_SIZE, x1=x0+CELL_SIZE;
            float z0=(z-GRID_H/2)*CELL_SIZE, z1=z0+CELL_SIZE;
            float y00=cornerHeight(x,z)-0.018f;
            float y10=cornerHeight(x+1,z)-0.018f;
            float y01=cornerHeight(x,z+1)-0.018f;
            float y11=cornerHeight(x+1,z+1)-0.018f;
            Color c00=colorAt(y00,x,z), c10=colorAt(y10,x+1,z);
            Color c01=colorAt(y01,x,z+1), c11=colorAt(y11,x+1,z+1);

            push(x0,y00,z0,c00); push(x1,y11,z1,c11); push(x1,y10,z0,c10);
            push(x0,y00,z0,c00); push(x0,y01,z1,c01); push(x1,y11,z1,c11);
        }
    }

    UploadMesh(&mesh,false);
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
        if (RoadAt(prop.x, prop.z) || CellOverlapsVisibleRoad(prop.x, prop.z) || zones_[prop.z][prop.x] != Zone::None) continue;
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
    Color asphalt{58, 64, 67, 255};

    bool n = RoadAt(x, z - 1), s = RoadAt(x, z + 1), e = RoadAt(x + 1, z), w = RoadAt(x - 1, z);
    int connections = (n ? 1 : 0) + (s ? 1 : 0) + (e ? 1 : 0) + (w ? 1 : 0);
    bool horizontal = e || w;
    bool vertical = n || s;
    bool turn = connections == 2 && horizontal && vertical;
    bool roundedNode = connections != 2 || turn;

    Color sidewalk{177, 177, 169, 255};
    if (detailed && roundedNode) {
        // Round sidewalk pad under endpoints, bends and junctions.
        DrawCylinder({p.x, y - 0.025f, p.z}, CELL_SIZE * 0.50f, CELL_SIZE * 0.50f, 0.09f, 20, sidewalk);
    }

    DrawCube({p.x, y, p.z}, CELL_SIZE * 0.86f, 0.10f, CELL_SIZE * 0.86f, asphalt);
    if (roundedNode) {
        DrawCylinder({p.x, y + 0.002f, p.z}, CELL_SIZE * 0.405f, CELL_SIZE * 0.405f, 0.105f, 20, asphalt);
    }

    Color line{218, 208, 160, 230};
    if (!detailed) {
        if (horizontal && !vertical) DrawCube({p.x, y + 0.06f, p.z}, CELL_SIZE * 0.50f, 0.018f, 0.035f, line);
        else if (vertical && !horizontal) DrawCube({p.x, y + 0.06f, p.z}, 0.035f, 0.018f, CELL_SIZE * 0.50f, line);
        return;
    }

    Color curb{148, 152, 149, 255};
    float sw = 0.18f;
    if (!n && !roundedNode) {
        DrawCube({p.x, y + 0.08f, p.z - CELL_SIZE * 0.43f}, CELL_SIZE * 0.88f, 0.08f, sw, sidewalk);
        DrawCube({p.x, y + 0.055f, p.z - CELL_SIZE * 0.34f}, CELL_SIZE * 0.88f, 0.07f, 0.035f, curb);
    }
    if (!s && !roundedNode) {
        DrawCube({p.x, y + 0.08f, p.z + CELL_SIZE * 0.43f}, CELL_SIZE * 0.88f, 0.08f, sw, sidewalk);
        DrawCube({p.x, y + 0.055f, p.z + CELL_SIZE * 0.34f}, CELL_SIZE * 0.88f, 0.07f, 0.035f, curb);
    }
    if (!w && !roundedNode) {
        DrawCube({p.x - CELL_SIZE * 0.43f, y + 0.08f, p.z}, sw, 0.08f, CELL_SIZE * 0.88f, sidewalk);
        DrawCube({p.x - CELL_SIZE * 0.34f, y + 0.055f, p.z}, 0.035f, 0.07f, CELL_SIZE * 0.88f, curb);
    }
    if (!e && !roundedNode) {
        DrawCube({p.x + CELL_SIZE * 0.43f, y + 0.08f, p.z}, sw, 0.08f, CELL_SIZE * 0.88f, sidewalk);
        DrawCube({p.x + CELL_SIZE * 0.34f, y + 0.055f, p.z}, 0.035f, 0.07f, CELL_SIZE * 0.88f, curb);
    }

    if (horizontal && !vertical) {
        DrawCube({p.x, y + 0.061f, p.z}, CELL_SIZE * 0.52f, 0.018f, 0.035f, line);
    } else if (vertical && !horizontal) {
        DrawCube({p.x, y + 0.061f, p.z}, 0.035f, 0.018f, CELL_SIZE * 0.52f, line);
    }

    if (HashCell(x, z, 90) % 23u == 0u && connections == 2 && !turn) {
        if (horizontal && !n) DrawStreetLight({p.x, p.y + 0.08f, p.z - CELL_SIZE * 0.43f}, true);
        else if (vertical && !w) DrawStreetLight({p.x - CELL_SIZE * 0.43f, p.y + 0.08f, p.z}, false);
    }
}

void City::DrawRoads(const Camera3D& camera) const {
    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.95f + 40.0f;
    float visibleSq = visibleRadius * visibleRadius;

    auto endpointIsJunction = [&](const RoadVisualPath& owner, bool atStart) {
        if (owner.points.size() < 2) return false;
        Vector3 p = atStart ? owner.points.front() : owner.points.back();
        float thresholdSq = (CELL_SIZE * 0.34f) * (CELL_SIZE * 0.34f);

        for (const auto& other : roadVisualPaths_) {
            if (&other == &owner || other.points.size() < 2) continue;

            for (size_t i = 0; i + 1 < other.points.size(); ++i) {
                const Vector3& a = other.points[i];
                const Vector3& b = other.points[i + 1];

                float abx = b.x - a.x;
                float abz = b.z - a.z;
                float lenSq = abx * abx + abz * abz;
                if (lenSq < 0.00001f) continue;

                float apx = p.x - a.x;
                float apz = p.z - a.z;
                float t = Clamp((apx * abx + apz * abz) / lenSq, 0.0f, 1.0f);
                float qx = a.x + abx * t;
                float qz = a.z + abz * t;
                float dx = p.x - qx;
                float dz = p.z - qz;

                if (dx * dx + dz * dz <= thresholdSq) return true;
            }
        }
        return false;
    };

    auto buildRibbon = [&](const std::vector<Vector3>& points, float halfWidth, float lift, Color col) {
        if (points.size() < 2) return;

        std::vector<Vector3> left(points.size());
        std::vector<Vector3> right(points.size());

        for (size_t i = 0; i < points.size(); ++i) {
            Vector3 p = points[i];
            Vector3 prevDir{}, nextDir{};

            if (i > 0) {
                prevDir = Vector3Subtract(points[i], points[i - 1]);
                prevDir.y = 0.0f;
                float l = sqrtf(prevDir.x * prevDir.x + prevDir.z * prevDir.z);
                if (l > 0.0001f) { prevDir.x /= l; prevDir.z /= l; }
            }
            if (i + 1 < points.size()) {
                nextDir = Vector3Subtract(points[i + 1], points[i]);
                nextDir.y = 0.0f;
                float l = sqrtf(nextDir.x * nextDir.x + nextDir.z * nextDir.z);
                if (l > 0.0001f) { nextDir.x /= l; nextDir.z /= l; }
            }

            Vector3 dir{};
            if (i == 0) dir = nextDir;
            else if (i + 1 == points.size()) dir = prevDir;
            else {
                dir = Vector3Add(prevDir, nextDir);
                float l = sqrtf(dir.x * dir.x + dir.z * dir.z);
                if (l < 0.001f) dir = nextDir;
                else { dir.x /= l; dir.z /= l; }
            }

            Vector3 normal{-dir.z, 0.0f, dir.x};
            float width = halfWidth;

            if (i > 0 && i + 1 < points.size()) {
                Vector3 nNext{-nextDir.z, 0.0f, nextDir.x};
                float denom = fabsf(normal.x * nNext.x + normal.z * nNext.z);
                denom = std::max(0.62f, denom);
                width = std::min(halfWidth / denom, halfWidth * 1.24f);
            }

            p.y += lift;
            left[i] = {p.x + normal.x * width, p.y, p.z + normal.z * width};
            right[i] = {p.x - normal.x * width, p.y, p.z - normal.z * width};
        }

        for (size_t i = 0; i + 1 < points.size(); ++i) {
            DrawTriangle3D(left[i], left[i + 1], right[i + 1], col);
            DrawTriangle3D(left[i], right[i + 1], right[i], col);
        }
    };

    auto drawCenterDashes = [&](const RoadVisualPath& path) {
        if (path.points.size() < 2) return;

        bool startJunction = endpointIsJunction(path, true);
        bool endJunction = endpointIsJunction(path, false);
        float clearLength = CELL_SIZE * 0.62f;

        float total = 0.0f;
        for (size_t i = 0; i + 1 < path.points.size(); ++i) {
            float dx = path.points[i + 1].x - path.points[i].x;
            float dz = path.points[i + 1].z - path.points[i].z;
            total += sqrtf(dx * dx + dz * dz);
        }

        float travelled = 0.0f;
        for (size_t i = 0; i + 1 < path.points.size(); ++i) {
            Vector3 a = path.points[i];
            Vector3 b = path.points[i + 1];
            Vector3 d = Vector3Subtract(b, a);
            d.y = 0.0f;
            float len = sqrtf(d.x * d.x + d.z * d.z);
            if (len < 0.001f) continue;

            float segStart = travelled;
            float segEnd = travelled + len;
            travelled = segEnd;

            if (startJunction && segStart < clearLength) continue;
            if (endJunction && segEnd > total - clearLength) continue;

            float phase = fmodf(segStart, 1.50f);
            if (phase > 0.72f) continue;

            d.x /= len; d.z /= len;
            Vector3 side{-d.z * 0.014f, 0.0f, d.x * 0.014f};
            a.y += 0.154f;
            b.y += 0.154f;

            DrawTriangle3D(
                {a.x + side.x, a.y, a.z + side.z},
                {b.x + side.x, b.y, b.z + side.z},
                {b.x - side.x, b.y, b.z - side.z},
                Color{226, 220, 190, 185}
            );
            DrawTriangle3D(
                {a.x + side.x, a.y, a.z + side.z},
                {b.x - side.x, b.y, b.z - side.z},
                {a.x - side.x, a.y, a.z - side.z},
                Color{226, 220, 190, 185}
            );
        }
    };

    Color sidewalk{171, 173, 169, 255};
    Color asphalt{47, 52, 56, 255};

    // Pass 1: every sidewalk. At crossings they merge with each other.
    for (const auto& path : roadVisualPaths_) {
        if (path.points.size() < 2) continue;
        Vector3 center = path.points[path.points.size() / 2];
        float dx = center.x - camera.target.x;
        float dz = center.z - camera.target.z;
        if (dx * dx + dz * dz > visibleSq) continue;

        buildRibbon(path.points, CELL_SIZE * 0.385f, 0.050f, sidewalk);
    }

    // Pass 2: every asphalt strip. Asphalt therefore always covers overlapping
    // sidewalks at a junction, producing one continuous road surface.
    for (const auto& path : roadVisualPaths_) {
        if (path.points.size() < 2) continue;
        Vector3 center = path.points[path.points.size() / 2];
        float dx = center.x - camera.target.x;
        float dz = center.z - camera.target.z;
        if (dx * dx + dz * dz > visibleSq) continue;

        buildRibbon(path.points, CELL_SIZE * 0.300f, 0.103f, asphalt);
    }

    // Pass 3: markings. No circles or special blobs are used at junctions.
    for (const auto& path : roadVisualPaths_) {
        if (path.points.size() < 2) continue;
        drawCenterDashes(path);
    }
}

void City::DrawZones(const Camera3D& camera) const {
    if (tool_ != Tool::Residential && tool_ != Tool::Commercial && tool_ != Tool::Industrial) return;

    float cameraDistance = Vector3Distance(camera.position, camera.target);
    float visibleRadius = cameraDistance * 1.70f + 28.0f;
    float visibleSq = visibleRadius * visibleRadius;

    for (int z = 0; z < GRID_H; ++z) {
        for (int x = 0; x < GRID_W; ++x) {
            if (zones_[z][x] == Zone::None || IsWater(x, z)) continue;
            if (!ZoneCellEligible(x, z) && BuildingIndexAt(x, z) < 0) continue;
            if (BuildingIndexAt(x, z) >= 0 || ServiceIndexAt(x, z) >= 0) continue;

            Vector3 p = GridToWorld(x, z);
            float dx = p.x - camera.target.x;
            float dz = p.z - camera.target.z;
            if (dx * dx + dz * dz > visibleSq) continue;

            Vector3 nearest{}, tangent{};
            DistanceToVisibleRoad(p.x, p.z, &nearest, &tangent);
            Vector3 toRoad{nearest.x - p.x, 0.0f, nearest.z - p.z};
            float angle = atan2f(toRoad.x, toRoad.z) * RAD2DEG;

            rlPushMatrix();
            rlTranslatef(p.x, 0.0f, p.z);
            rlRotatef(angle, 0.0f, 1.0f, 0.0f);
            rlTranslatef(-p.x, 0.0f, -p.z);
            DrawCube(
                {p.x, p.y + 0.012f, p.z},
                CELL_SIZE * 0.76f,
                0.025f,
                CELL_SIZE * 0.76f,
                ZoneColor(zones_[z][x], 70)
            );
            rlPopMatrix();
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

        rlPushMatrix();
        rlTranslatef(center.x, 0.0f, center.z);
        rlRotatef(b.rotationDegrees, 0.0f, 1.0f, 0.0f);
        rlTranslatef(-center.x, 0.0f, -center.z);

        BuildingRenderer::DrawBuilding(
            b,
            center,
            b.w * CELL_SIZE,
            b.d * CELL_SIZE,
            d2 <= detailSq
        );

        rlPopMatrix();
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

void City::DrawTraffic(const Camera3D& camera) const {
    float camDist = Vector3Distance(camera.position, camera.target);
    float visibleRadius = camDist * 1.55f + 24.0f;
    float visibleSq = visibleRadius * visibleRadius;

    for (const auto& v : vehicles_) {
        Vector3 a=GridToWorld(v.x,v.z);
        Vector3 b=GridToWorld(v.nextX,v.nextZ);
        Vector3 pos{
            a.x+(b.x-a.x)*v.t,
            a.y+(b.y-a.y)*v.t+0.22f,
            a.z+(b.z-a.z)*v.t
        };

        float dx=pos.x-camera.target.x, dz=pos.z-camera.target.z;
        if(dx*dx+dz*dz>visibleSq) continue;

        Vector3 dir{b.x-a.x,0.0f,b.z-a.z};
        float len=sqrtf(dir.x*dir.x+dir.z*dir.z);
        if(len<0.001f) continue;
        dir.x/=len; dir.z/=len;

        // Two-way lane offset.
        Vector3 perp{-dir.z,0.0f,dir.x};
        pos.x += perp.x*0.15f;
        pos.z += perp.z*0.15f;

        Vector3 front{pos.x+dir.x*0.23f,pos.y,pos.z+dir.z*0.23f};
        Vector3 rear {pos.x-dir.x*0.23f,pos.y,pos.z-dir.z*0.23f};
        DrawCylinderEx(rear,front,0.13f,0.13f,8,v.color);
        DrawSphereEx({pos.x,pos.y+0.10f,pos.z},0.105f,3,7,Color{113,153,170,255});
    }
}

void City::DrawRoadPreview() const {
    if (tool_ != Tool::Road || !dragging_) return;

    std::vector<Vector3> curve = BuildDraggedRoadCurve();
    if (curve.size() < 2) return;

    bool valid = true;
    for (const auto& p : curve) {
        if (!RoadCurvePointValid(p)) {
            valid = false;
            break;
        }
    }

    auto ribbon = [&](float halfWidth, float lift, Color col) {
        std::vector<Vector3> left(curve.size());
        std::vector<Vector3> right(curve.size());

        for (size_t i = 0; i < curve.size(); ++i) {
            Vector3 p = curve[i];
            Vector3 prevDir{}, nextDir{};

            if (i > 0) {
                prevDir = Vector3Subtract(curve[i], curve[i - 1]);
                prevDir.y = 0.0f;
                float l = sqrtf(prevDir.x * prevDir.x + prevDir.z * prevDir.z);
                if (l > 0.0001f) { prevDir.x /= l; prevDir.z /= l; }
            }
            if (i + 1 < curve.size()) {
                nextDir = Vector3Subtract(curve[i + 1], curve[i]);
                nextDir.y = 0.0f;
                float l = sqrtf(nextDir.x * nextDir.x + nextDir.z * nextDir.z);
                if (l > 0.0001f) { nextDir.x /= l; nextDir.z /= l; }
            }

            Vector3 dir{};
            if (i == 0) dir = nextDir;
            else if (i + 1 == curve.size()) dir = prevDir;
            else {
                dir = Vector3Add(prevDir, nextDir);
                float l = sqrtf(dir.x * dir.x + dir.z * dir.z);
                if (l < 0.001f) dir = nextDir;
                else { dir.x /= l; dir.z /= l; }
            }

            Vector3 normal{-dir.z, 0.0f, dir.x};
            float width = halfWidth;

            if (i > 0 && i + 1 < curve.size()) {
                Vector3 nNext{-nextDir.z, 0.0f, nextDir.x};
                float denom = fabsf(normal.x * nNext.x + normal.z * nNext.z);
                denom = std::max(0.55f, denom);
                width = std::min(halfWidth / denom, halfWidth * 1.35f);
            }

            p.y += lift;
            left[i] = {p.x + normal.x * width, p.y, p.z + normal.z * width};
            right[i] = {p.x - normal.x * width, p.y, p.z - normal.z * width};
        }

        for (size_t i = 0; i + 1 < curve.size(); ++i) {
            DrawTriangle3D(left[i], left[i + 1], right[i + 1], col);
            DrawTriangle3D(left[i], right[i + 1], right[i], col);
        }
    };

    Color outer = valid ? Color{91, 192, 214, 120} : Color{229, 91, 77, 130};
    Color inner = valid ? Color{62, 137, 158, 180} : Color{176, 55, 48, 188};

    ribbon(CELL_SIZE * 0.385f, 0.116f, outer);
    ribbon(CELL_SIZE * 0.295f, 0.128f, inner);
}

void City::DrawPlacementPreview() const {
    if (tool_ == Tool::Road && dragging_) return;
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
    DrawRoadPreview();
    DrawTraffic(camera);
    DrawServices(camera);
    DrawBuildings(camera);
    DrawPlacementPreview();
}

void City::DrawBar(int x, int y, int w, float value, Color color) const {
    value = Clamp(value, 0.0f, 1.0f);
    DrawRectangleRounded({(float)x, (float)y, (float)w, 8.0f}, 0.45f, 6, Color{31, 40, 45, 245});
    if (value > 0.001f) {
        DrawRectangleRounded({(float)x, (float)y, (float)std::max(2, (int)(w * value)), 8.0f}, 0.45f, 6, color);
    }
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
    const int w = GetScreenWidth();
    DrawRectangle(0, 0, w, 42, Color{17, 27, 32, 240});

    DrawText("CITY LAB", 18, 10, 20, Color{239, 244, 245, 255});
    DrawText("v0.15", 116, 14, 12, Color{103, 184, 205, 255});

    std::string level = "STADTSTUFE " + std::to_string(cityLevel_);
    DrawText(level.c_str(), 178, 14, 12, Color{179, 201, 207, 255});

    std::string score = "SCORE " + std::to_string(cityScore_);
    DrawText(score.c_str(), 292, 14, 12, cityScore_ >= 70 ? Color{105, 201, 127, 255} : Color{220, 184, 92, 255});

    std::string traffic = "VERKEHR " + std::to_string(vehicles_.size());
    DrawText(traffic.c_str(), 380, 14, 11, Color{153, 181, 188, 255});

    std::string date = std::to_string(day_) + "." + std::to_string(month_) + "." + std::to_string(year_);
    int dateW = MeasureText(date.c_str(), 13);
    DrawText(date.c_str(), w / 2 - dateW / 2, 14, 13, Color{193, 207, 211, 255});

    std::string fps = "FPS " + std::to_string(GetFPS());
    Color fpsColor = GetFPS() >= 45 ? Color{111, 199, 135, 255} : Color{232, 144, 91, 255};
    DrawText(fps.c_str(), w - MeasureText(fps.c_str(), 12) - 18, 15, 12, fpsColor);
}

void City::DrawToolPanel() const {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const float hudTop = (float)sh - 136.0f;

    DrawRectangle(0, (int)hudTop, sw, 136, Color{20, 31, 36, 246});
    DrawRectangle(0, (int)hudTop, sw, 1, Color{68, 101, 112, 255});

    const char* names[8] = {
        "STRASSE", "WOHNEN", "GEWERBE", "INDUSTRIE",
        "ABRISS", "STROM", "WASSER", "PARK"
    };
    const char* prices[8] = {
        "CHF 120", "ZONE", "ZONE", "ZONE",
        "ENTFERNEN", "CHF 6500", "CHF 3600", "CHF 1200"
    };
    const Color accents[8] = {
        Color{151, 168, 175, 255}, Color{71, 190, 108, 255},
        Color{65, 146, 232, 255}, Color{228, 177, 61, 255},
        Color{218, 92, 83, 255}, Color{232, 192, 65, 255},
        Color{72, 161, 222, 255}, Color{92, 184, 103, 255}
    };

    const float gap = 7.0f;
    float toolW = ((float)sw - 32.0f - gap * 7.0f) / 8.0f;
    toolW = Clamp(toolW, 88.0f, 145.0f);
    float totalW = toolW * 8.0f + gap * 7.0f;
    float startX = ((float)sw - totalW) * 0.5f;
    float y = hudTop + 70.0f;

    for (int i = 0; i < 8; ++i) {
        Rectangle r{startX + i * (toolW + gap), y, toolW, 54.0f};
        bool selected = (int)tool_ == i;
        Color bg = selected ? Color{55, 77, 85, 255} : Color{31, 44, 50, 255};

        DrawRectangleRounded(r, 0.34f, 10, bg);
        if (selected) DrawRectangleLinesEx(r, 1.6f, accents[i]);

        Vector2 icon{r.x + 20.0f, r.y + 19.0f};
        DrawCircleV(icon, 11.0f, selected ? accents[i] : Color{58, 74, 80, 255});
        std::string key = std::to_string(i + 1);
        DrawText(key.c_str(), (int)icon.x - 3, (int)icon.y - 5, 10, Color{245, 247, 247, 255});

        int nameSize = toolW < 108.0f ? 10 : 11;
        DrawText(names[i], (int)r.x + 38, (int)r.y + 11, nameSize, Color{236, 241, 242, 255});
        DrawText(prices[i], (int)r.x + 12, (int)r.y + 35, 9,
                 selected ? Color{196, 221, 227, 255} : Color{139, 155, 161, 255});
    }
}

void City::DrawDemandPanel() const {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const int y = sh - 125;
    const int x = (int)(sw * 0.42f) + 8;
    const int groupW = (int)(sw * 0.22f) - 16;

    DrawText("NACHFRAGE", x, y, 9, Color{137, 155, 161, 255});
    int barW = std::max(34, (groupW - 70) / 3);

    int bx = x + 72;
    DrawText("W", bx - 14, y, 10, Color{83, 205, 117, 255});
    DrawBar(bx, y + 2, barW, residentialDemand_, Color{71, 190, 108, 255});

    bx += barW + 30;
    DrawText("G", bx - 14, y, 10, Color{89, 163, 236, 255});
    DrawBar(bx, y + 2, barW, commercialDemand_, Color{65, 146, 232, 255});

    bx += barW + 30;
    DrawText("I", bx - 12, y, 10, Color{234, 188, 72, 255});
    DrawBar(bx, y + 2, barW, industrialDemand_, Color{228, 177, 61, 255});
}

void City::DrawInfoPanel() const {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const float hudTop = (float)sh - 136.0f;
    const int y = (int)hudTop + 13;

    int netValue = projectedIncome_ - projectedExpenses_;
    std::string money = "CHF " + std::to_string(money_);
    std::string pop = std::to_string(population_) + " Einwohner";
    std::string jobs = std::to_string(filledJobs_) + "/" + std::to_string(totalJobs_) + " Jobs";
    std::string net = std::string(netValue >= 0 ? "+" : "") + std::to_string(netValue) + " / Monat";

    // Left 42%: economy / population.
    DrawText(money.c_str(), 18, y, 14, financialWarning_ ? Color{236, 105, 85, 255} : Color{235, 241, 242, 255});
    DrawText(pop.c_str(), 132, y, 11, Color{198, 213, 217, 255});
    DrawText(jobs.c_str(), 258, y, 11, Color{198, 213, 217, 255});
    DrawText(net.c_str(), 365, y, 11, netValue >= 0 ? Color{103, 201, 126, 255} : Color{229, 112, 92, 255});

    // Right service column ends before speed controls.
    int ux = (int)(sw * 0.64f) + 10;
    std::string power = "STROM " + std::to_string(powerUsed_) + "/" + std::to_string(powerCapacity_);
    std::string water = "WASSER " + std::to_string(waterUsed_) + "/" + std::to_string(waterCapacity_);
    std::string happy = "GLUECK " + std::to_string((int)(happiness_ * 100.0f)) + "%";

    DrawText(power.c_str(), ux, y, 10,
             powerCapacity_ > 0 && powerUsed_ <= powerCapacity_ ? Color{226, 198, 83, 255} : Color{229, 104, 84, 255});
    DrawText(water.c_str(), ux + 92, y, 10,
             waterCapacity_ > 0 && waterUsed_ <= waterCapacity_ ? Color{80, 164, 229, 255} : Color{229, 104, 84, 255});
    DrawText(happy.c_str(), ux + 192, y, 10, Color{159, 207, 172, 255});

    const char* speedLabels[4] = {"II", "1x", "2x", "3x"};
    for (int i = 0; i < 4; ++i) {
        Rectangle r{(float)sw - 206.0f + i * 46.0f, hudTop + 10.0f, 40.0f, 32.0f};
        bool selected = gameSpeed_ == i;
        DrawRectangleRounded(r, 0.48f, 12, selected ? Color{69, 104, 116, 255} : Color{34, 48, 54, 255});
        if (selected) DrawRectangleLinesEx(r, 1.2f, Color{128, 188, 204, 255});
        DrawText(speedLabels[i], (int)r.x + 10, (int)r.y + 9, 11, Color{235, 240, 241, 255});
    }
}

void City::DrawProgressPanel() const {
    const int sw = GetScreenWidth();
    const float progress = milestoneTarget_ > 0 ? Clamp((float)population_ / milestoneTarget_, 0.0f, 1.0f) : 1.0f;

    Rectangle card{16.0f, 52.0f, 268.0f, 48.0f};
    DrawRectangleRounded(card, 0.30f, 10, Color{20, 31, 36, 205});
    std::string title = "Stadtstufe " + std::to_string(cityLevel_) + "  |  Score " + std::to_string(cityScore_) + "/100";
    DrawText(title.c_str(), 29, 62, 11, Color{224, 234, 236, 255});

    if (milestoneTarget_ < 900000) {
        std::string goal = "Naechstes Ziel: " + std::to_string(population_) + " / " + std::to_string(milestoneTarget_);
        DrawText(goal.c_str(), 29, 80, 10, Color{161, 181, 187, 255});
        DrawBar(150, 82, 116, progress, Color{86, 176, 197, 255});
    } else {
        DrawText("Metropole erreicht", 29, 80, 10, Color{110, 204, 132, 255});
    }

    if (milestoneBannerDays_ > 0 && lastMilestoneReward_ > 0) {
        std::string msg = "MEILENSTEIN! Stadtstufe " + std::to_string(cityLevel_) +
                          "  +CHF " + std::to_string(lastMilestoneReward_);
        int tw = MeasureText(msg.c_str(), 13);
        Rectangle banner{sw * 0.5f - tw * 0.5f - 18.0f, 55.0f, (float)tw + 36.0f, 32.0f};
        DrawRectangleRounded(banner, 0.48f, 12, Color{43, 75, 65, 235});
        DrawText(msg.c_str(), (int)banner.x + 18, (int)banner.y + 9, 13, Color{159, 230, 176, 255});
    }

    if (financialWarning_) {
        const char* msg = "Budget negativ - reduziere Unterhalt oder baue mehr Steuerbasis.";
        int tw = MeasureText(msg, 10);
        Rectangle warn{(float)sw - tw - 32.0f, 54.0f, (float)tw + 18.0f, 25.0f};
        DrawRectangleRounded(warn, 0.40f, 10, Color{82, 43, 40, 225});
        DrawText(msg, (int)warn.x + 9, (int)warn.y + 7, 10, Color{242, 155, 137, 255});
    }
}

void City::DrawUI() const {
    DrawTopBar();
    DrawToolPanel();
    DrawDemandPanel();
    DrawInfoPanel();
    DrawProgressPanel();
}
