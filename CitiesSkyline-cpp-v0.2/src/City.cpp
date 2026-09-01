#include "City.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

static Color ZoneColor(Zone zone, unsigned char alpha) {
    switch (zone) {
        case Zone::Residential: return Color{82, 190, 110, alpha};
        case Zone::Commercial:  return Color{70, 145, 235, alpha};
        case Zone::Industrial:  return Color{225, 180, 65, alpha};
        default: return Color{0,0,0,0};
    }
}

City::City() {
    // Kleine Startstrasse, damit sofort klar ist, wie das System funktioniert.
    for (int x = 27; x <= 36; ++x) roads_[32][x] = true;
    roadCount_ = 10;
}

bool City::Inside(int x, int z) const {
    return x >= 0 && x < GRID_W && z >= 0 && z < GRID_H;
}

Vector3 City::GridToWorld(int x, int z) const {
    return {
        (x - GRID_W / 2) * CELL_SIZE + CELL_SIZE * 0.5f,
        0.0f,
        (z - GRID_H / 2) * CELL_SIZE + CELL_SIZE * 0.5f
    };
}

bool City::MouseToGrid(const Camera3D& camera, int& gx, int& gz) const {
    Ray ray = GetMouseRay(GetMousePosition(), camera);
    if (fabsf(ray.direction.y) < 0.00001f) return false;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0.0f) return false;
    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    gx = (int)floorf(hit.x / CELL_SIZE + GRID_W / 2.0f);
    gz = (int)floorf(hit.z / CELL_SIZE + GRID_H / 2.0f);
    return Inside(gx, gz);
}

bool City::AdjacentToRoad(int x, int z) const {
    static const int dx[4] = {1,-1,0,0};
    static const int dz[4] = {0,0,1,-1};
    for (int i=0;i<4;i++) {
        int nx=x+dx[i], nz=z+dz[i];
        if (Inside(nx,nz) && roads_[nz][nx]) return true;
    }
    return false;
}

int City::BuildingIndexAt(int x, int z) const {
    for (int i=0;i<(int)buildings_.size();i++) {
        if (buildings_[i].x == x && buildings_[i].z == z) return i;
    }
    return -1;
}

bool City::HasBuilding(int x, int z) const { return BuildingIndexAt(x,z) >= 0; }

void City::Update(float dt, const Camera3D& camera) {
    HandleInput(camera);
    if (!paused_) Simulate(dt);
}

void City::HandleInput(const Camera3D& camera) {
    if (IsKeyPressed(KEY_ONE)) tool_ = Tool::Road;
    if (IsKeyPressed(KEY_TWO)) tool_ = Tool::Residential;
    if (IsKeyPressed(KEY_THREE)) tool_ = Tool::Commercial;
    if (IsKeyPressed(KEY_FOUR)) tool_ = Tool::Industrial;
    if (IsKeyPressed(KEY_FIVE) || IsKeyPressed(KEY_B)) tool_ = Tool::Bulldoze;
    if (IsKeyPressed(KEY_SPACE)) paused_ = !paused_;

    int gx=-1,gz=-1;
    if (MouseToGrid(camera,gx,gz)) { hoverX_=gx; hoverZ_=gz; }
    else { hoverX_=-1; hoverZ_=-1; }

    // Linksklick in UI-Leiste nicht als Weltklick behandeln.
    if (GetMousePosition().y < 68.0f) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverX_ >= 0) {
        if (tool_ == Tool::Bulldoze) {
            BulldozeAt(hoverX_,hoverZ_);
        } else {
            dragging_ = true;
            dragStartX_ = hoverX_;
            dragStartZ_ = hoverZ_;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && dragging_) {
        if (hoverX_ >= 0) {
            if (tool_ == Tool::Road) PlaceRoadLine(dragStartX_,dragStartZ_,hoverX_,hoverZ_);
            else if (tool_ == Tool::Residential) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Residential);
            else if (tool_ == Tool::Commercial) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Commercial);
            else if (tool_ == Tool::Industrial) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Industrial);
        }
        dragging_ = false;
    }
}

void City::PlaceRoadLine(int x0,int z0,int x1,int z1) {
    int dx = abs(x1-x0), dz = abs(z1-z0);
    if (dx >= dz) {
        int lo=std::min(x0,x1), hi=std::max(x0,x1);
        for (int x=lo;x<=hi;x++) {
            if (!Inside(x,z0) || roads_[z0][x] || money_ < 100) continue;
            int bi = BuildingIndexAt(x,z0);
            if (bi >= 0) buildings_.erase(buildings_.begin()+bi);
            roads_[z0][x]=true; zones_[z0][x]=Zone::None; money_-=100; roadCount_++;
        }
    } else {
        int lo=std::min(z0,z1), hi=std::max(z0,z1);
        for (int z=lo;z<=hi;z++) {
            if (!Inside(x0,z) || roads_[z][x0] || money_ < 100) continue;
            int bi = BuildingIndexAt(x0,z);
            if (bi >= 0) buildings_.erase(buildings_.begin()+bi);
            roads_[z][x0]=true; zones_[z][x0]=Zone::None; money_-=100; roadCount_++;
        }
    }
    RecalculateStats();
}

void City::PaintZoneRect(int x0,int z0,int x1,int z1,Zone zone) {
    int minX=std::min(x0,x1), maxX=std::max(x0,x1);
    int minZ=std::min(z0,z1), maxZ=std::max(z0,z1);
    for (int z=minZ;z<=maxZ;z++) for (int x=minX;x<=maxX;x++) {
        if (!Inside(x,z) || roads_[z][x]) continue;
        zones_[z][x]=zone;
    }
}

void City::BulldozeAt(int x,int z) {
    int bi=BuildingIndexAt(x,z);
    if (bi >= 0) {
        buildings_.erase(buildings_.begin()+bi);
        money_ -= 25;
    } else if (roads_[z][x]) {
        roads_[z][x]=false;
        roadCount_=std::max(0,roadCount_-1);
        money_ -= 20;
    } else if (zones_[z][x] != Zone::None) {
        zones_[z][x]=Zone::None;
    }
    RecalculateStats();
}

void City::Simulate(float dt) {
    simAccumulator_ += dt;
    spawnAccumulator_ += dt;

    if (spawnAccumulator_ >= 0.28f) {
        spawnAccumulator_ = 0.0f;
        TrySpawnBuilding();
    }

    if (simAccumulator_ >= 1.0f) {
        simAccumulator_ = 0.0f;
        RecalculateStats();
        RecalculateDemand();
        int taxIncome = population_ * 2 + jobs_ * 3;
        int upkeep = roadCount_ * 1;
        money_ += taxIncome - upkeep;

        // Kleine Chance auf Gebäude-Upgrade bei gesunder Nachfrage.
        if (!buildings_.empty() && GetRandomValue(0,99) < 18) {
            Building& b = buildings_[GetRandomValue(0,(int)buildings_.size()-1)];
            if (b.level < 3) {
                float relevant = b.zone==Zone::Residential ? residentialDemand_ : (b.zone==Zone::Commercial ? commercialDemand_ : industrialDemand_);
                if (relevant > 0.5f) {
                    b.level++;
                    b.size.y += 0.7f;
                    if (b.zone==Zone::Residential) b.residents += 5;
                    else b.jobs += 4;
                }
            }
        }
    }
}

void City::TrySpawnBuilding() {
    for (int attempt=0; attempt<35; attempt++) {
        int x=GetRandomValue(0,GRID_W-1), z=GetRandomValue(0,GRID_H-1);
        Zone zone=zones_[z][x];
        if (zone==Zone::None || roads_[z][x] || HasBuilding(x,z) || !AdjacentToRoad(x,z)) continue;

        float demand = zone==Zone::Residential ? residentialDemand_ : (zone==Zone::Commercial ? commercialDemand_ : industrialDemand_);
        if ((float)GetRandomValue(0,100)/100.0f > demand) continue;

        Building b;
        b.x=x; b.z=z; b.zone=zone; b.level=1;
        b.size = {1.45f, 1.2f + GetRandomValue(0,8)*0.1f, 1.45f};
        if (zone==Zone::Residential) b.residents=GetRandomValue(5,12);
        else if (zone==Zone::Commercial) b.jobs=GetRandomValue(5,10);
        else b.jobs=GetRandomValue(8,15);
        buildings_.push_back(b);
        return;
    }
}

void City::RecalculateStats() {
    population_=0; jobs_=0;
    for (const auto& b: buildings_) { population_+=b.residents; jobs_+=b.jobs; }
}

void City::RecalculateDemand() {
    float housingPressure = jobs_ <= 0 ? 0.65f : Clamp((float)(jobs_+20)/(float)(population_+30),0.15f,1.0f);
    float jobPressure = population_ <= 0 ? 0.35f : Clamp((float)(population_+20)/(float)(jobs_+35),0.15f,1.0f);
    residentialDemand_ = Clamp(0.30f + housingPressure*0.65f,0.08f,1.0f);
    commercialDemand_ = Clamp(0.20f + (population_/220.0f) - (jobs_/900.0f),0.08f,0.95f);
    industrialDemand_ = Clamp(0.22f + jobPressure*0.55f,0.08f,0.95f);
}

void City::Draw3D(const Camera3D& camera) const {
    (void)camera;
    Color grassA{67,103,72,255}, grassB{70,108,76,255};
    for (int z=0;z<GRID_H;z++) for (int x=0;x<GRID_W;x++) {
        Vector3 p=GridToWorld(x,z);
        Color grass=((x+z)%2==0)?grassA:grassB;
        DrawCube({p.x,-0.035f,p.z},CELL_SIZE*0.985f,0.07f,CELL_SIZE*0.985f,grass);

        if (zones_[z][x]!=Zone::None && !roads_[z][x] && !HasBuilding(x,z)) {
            Color c=ZoneColor(zones_[z][x],85);
            DrawCube({p.x,0.015f,p.z},CELL_SIZE*0.88f,0.035f,CELL_SIZE*0.88f,c);
        }

        if (roads_[z][x]) {
            DrawCube({p.x,0.025f,p.z},CELL_SIZE*0.98f,0.08f,CELL_SIZE*0.98f,Color{70,73,76,255});
            bool horizontal=(Inside(x-1,z)&&roads_[z][x-1])||(Inside(x+1,z)&&roads_[z][x+1]);
            bool vertical=(Inside(x,z-1)&&roads_[z-1][x])||(Inside(x,z+1)&&roads_[z+1][x]);
            if (horizontal) DrawCube({p.x,0.075f,p.z},CELL_SIZE*0.58f,0.012f,0.045f,Color{205,190,118,255});
            if (vertical) DrawCube({p.x,0.075f,p.z},0.045f,0.012f,CELL_SIZE*0.58f,Color{205,190,118,255});
        }
    }

    for (const auto& b: buildings_) {
        Vector3 p=GridToWorld(b.x,b.z);
        Color base=ZoneColor(b.zone,255);
        float height=b.size.y;
        DrawCube({p.x,height*0.5f+0.08f,p.z},b.size.x,height,b.size.z,base);
        DrawCubeWires({p.x,height*0.5f+0.08f,p.z},b.size.x,height,b.size.z,Color{35,40,42,255});
        if (b.level>=2) DrawCube({p.x,height+0.18f,p.z},b.size.x*0.72f,0.22f,b.size.z*0.72f,Color{185,190,185,255});
    }

    if (hoverX_>=0 && hoverZ_>=0) {
        Vector3 p=GridToWorld(hoverX_,hoverZ_);
        Color c = tool_==Tool::Bulldoze ? RED : YELLOW;
        DrawCubeWires({p.x,0.13f,p.z},CELL_SIZE*0.94f,0.18f,CELL_SIZE*0.94f,c);
    }
}

void City::DrawUI() const {
    int w=GetScreenWidth();
    DrawRectangle(0,0,w,64,Color{18,21,23,245});
    DrawText("CITY LAB",20,17,25,RAYWHITE);

    std::string money="CHF " + std::to_string(money_);
    std::string pop="Einwohner  " + std::to_string(population_);
    std::string jobs="Jobs  " + std::to_string(jobs_);
    DrawText(money.c_str(),180,20,18,Color{230,232,230,255});
    DrawText(pop.c_str(),330,20,18,Color{230,232,230,255});
    DrawText(jobs.c_str(),500,20,18,Color{230,232,230,255});
    DrawText(paused_?"PAUSIERT":"LAEUFT",w-115,20,17,paused_?Color{235,175,78,255}:Color{112,205,132,255});

    Rectangle panel{18.0f,82.0f,220.0f,276.0f};
    DrawRectangleRounded(panel,0.07f,8,Color{18,21,23,235});
    DrawText("BAUEN",34,98,19,RAYWHITE);
    const char* labels[5]={"1  Strasse","2  Wohnen","3  Gewerbe","4  Industrie","5  Bulldozer"};
    for (int i=0;i<5;i++) {
        Rectangle r{31.0f,130.0f+i*42.0f,194.0f,33.0f};
        Color bg=((int)tool_==i)?Color{70,88,82,255}:Color{37,42,44,255};
        DrawRectangleRounded(r,0.15f,6,bg);
        DrawText(labels[i],43,(int)r.y+8,16,RAYWHITE);
    }

    DrawText("WASD bewegen",31,349,14,Color{190,195,194,255});
    DrawText("Rechtsziehen drehen  |  Mausrad zoom",31,370,14,Color{190,195,194,255});
    DrawText("SPACE pausieren",31,391,14,Color{190,195,194,255});

    Rectangle demand{(float)w-244.0f,82.0f,226.0f,148.0f};
    DrawRectangleRounded(demand,0.07f,8,Color{18,21,23,235});
    DrawText("NACHFRAGE",w-226,99,18,RAYWHITE);
    const char* dlabels[3]={"Wohnen","Gewerbe","Industrie"};
    float vals[3]={residentialDemand_,commercialDemand_,industrialDemand_};
    Color cols[3]={Color{82,190,110,255},Color{70,145,235,255},Color{225,180,65,255}};
    for(int i=0;i<3;i++) {
        int y=132+i*30;
        DrawText(dlabels[i],w-226,y,14,Color{210,214,212,255});
        DrawRectangle(w-150,y+2,112,12,Color{45,48,50,255});
        DrawRectangle(w-150,y+2,(int)(112*vals[i]),12,cols[i]);
    }

    if (dragging_ && hoverX_>=0) {
        const char* text = tool_==Tool::Road ? "Loslassen: Strasse bauen" : "Loslassen: Zone markieren";
        int tw=MeasureText(text,16);
        DrawRectangle(w/2-tw/2-12,74,tw+24,34,Color{18,21,23,220});
        DrawText(text,w/2-tw/2,83,16,RAYWHITE);
    }
}
