#include "City.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <string>

static Color ZoneColor(Zone zone, unsigned char alpha) {
    switch (zone) {
        case Zone::Residential: return Color{78, 188, 108, alpha};
        case Zone::Commercial: return Color{65, 143, 228, alpha};
        case Zone::Industrial: return Color{223, 176, 65, alpha};
        default: return Color{0,0,0,0};
    }
}

static Color Shift(Color c, int d) {
    auto clampc=[](int v){ return (unsigned char)std::max(0,std::min(255,v)); };
    return Color{clampc(c.r+d),clampc(c.g+d),clampc(c.b+d),c.a};
}

City::City() {
    for (int x=32; x<=47; ++x) {
        if (!IsWater(x,40)) { roads_[40][x]=true; roadCount_++; }
    }
}

bool City::Inside(int x,int z) const { return x>=0 && x<GRID_W && z>=0 && z<GRID_H; }

float City::TerrainHeight(int x,int z) const {
    float nx=(x-GRID_W*0.5f)/(GRID_W*0.5f);
    float nz=(z-GRID_H*0.5f)/(GRID_H*0.5f);
    float rolling = 0.42f*sinf(nx*5.2f) + 0.30f*cosf(nz*4.6f) + 0.20f*sinf((nx+nz)*8.0f);
    float edge = 0.65f*powf(std::max(fabsf(nx),fabsf(nz)),2.2f);
    float lakeDx=nx-0.42f, lakeDz=nz+0.18f;
    float lake = 2.35f*expf(-(lakeDx*lakeDx*8.5f + lakeDz*lakeDz*13.0f));
    float river = 1.55f*expf(-powf(nz - 0.22f*sinf(nx*4.0f)-0.30f,2.0f)*95.0f);
    return 0.65f + rolling + edge - lake - river;
}

bool City::IsWater(int x,int z) const { return TerrainHeight(x,z) < WATER_LEVEL; }

Vector3 City::GridToWorld(int x,int z) const {
    return {(x-GRID_W/2)*CELL_SIZE+CELL_SIZE*0.5f, TerrainHeight(x,z), (z-GRID_H/2)*CELL_SIZE+CELL_SIZE*0.5f};
}

bool City::MouseToGrid(const Camera3D& camera,int& gx,int& gz) const {
    Ray ray=GetMouseRay(GetMousePosition(),camera);
    if (fabsf(ray.direction.y)<0.00001f) return false;
    float t=(0.35f-ray.position.y)/ray.direction.y;
    if(t<0.0f) return false;
    Vector3 hit=Vector3Add(ray.position,Vector3Scale(ray.direction,t));
    gx=(int)floorf(hit.x/CELL_SIZE+GRID_W/2.0f);
    gz=(int)floorf(hit.z/CELL_SIZE+GRID_H/2.0f);
    return Inside(gx,gz);
}

bool City::AdjacentToRoad(int x,int z) const {
    static const int dx[4]={1,-1,0,0}; static const int dz[4]={0,0,1,-1};
    for(int i=0;i<4;i++){int nx=x+dx[i],nz=z+dz[i]; if(Inside(nx,nz)&&roads_[nz][nx]) return true;}
    return false;
}

int City::BuildingIndexAt(int x,int z) const {
    for(int i=0;i<(int)buildings_.size();++i) if(buildings_[i].x==x&&buildings_[i].z==z) return i;
    return -1;
}
bool City::HasBuilding(int x,int z) const { return BuildingIndexAt(x,z)>=0; }

void City::Update(float dt,const Camera3D& camera){ HandleInput(camera); if(!paused_) Simulate(dt); }

void City::HandleInput(const Camera3D& camera){
    if(IsKeyPressed(KEY_ONE))tool_=Tool::Road;
    if(IsKeyPressed(KEY_TWO))tool_=Tool::Residential;
    if(IsKeyPressed(KEY_THREE))tool_=Tool::Commercial;
    if(IsKeyPressed(KEY_FOUR))tool_=Tool::Industrial;
    if(IsKeyPressed(KEY_FIVE)||IsKeyPressed(KEY_B))tool_=Tool::Bulldoze;
    if(IsKeyPressed(KEY_SPACE))paused_=!paused_;
    int gx=-1,gz=-1; if(MouseToGrid(camera,gx,gz)){hoverX_=gx;hoverZ_=gz;}else{hoverX_=-1;hoverZ_=-1;}
    Vector2 m=GetMousePosition(); if(m.y<68.0f || (m.x<250.0f&&m.y<430.0f)) return;
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&hoverX_>=0){
        if(tool_==Tool::Bulldoze) BulldozeAt(hoverX_,hoverZ_);
        else {dragging_=true;dragStartX_=hoverX_;dragStartZ_=hoverZ_;}
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)&&dragging_){
        if(hoverX_>=0){
            if(tool_==Tool::Road) PlaceRoadLine(dragStartX_,dragStartZ_,hoverX_,hoverZ_);
            else if(tool_==Tool::Residential) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Residential);
            else if(tool_==Tool::Commercial) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Commercial);
            else if(tool_==Tool::Industrial) PaintZoneRect(dragStartX_,dragStartZ_,hoverX_,hoverZ_,Zone::Industrial);
        }
        dragging_=false;
    }
}

void City::PlaceRoadLine(int x0,int z0,int x1,int z1){
    int dx=abs(x1-x0),dz=abs(z1-z0);
    auto place=[&](int x,int z){
        if(!Inside(x,z)||roads_[z][x]||money_<100||IsWater(x,z)) return;
        int bi=BuildingIndexAt(x,z); if(bi>=0)buildings_.erase(buildings_.begin()+bi);
        roads_[z][x]=true; zones_[z][x]=Zone::None; money_-=100; roadCount_++;
    };
    if(dx>=dz){for(int x=std::min(x0,x1);x<=std::max(x0,x1);++x)place(x,z0);}else{for(int z=std::min(z0,z1);z<=std::max(z0,z1);++z)place(x0,z);}
    RecalculateStats();
}

void City::PaintZoneRect(int x0,int z0,int x1,int z1,Zone zone){
    for(int z=std::min(z0,z1);z<=std::max(z0,z1);++z)for(int x=std::min(x0,x1);x<=std::max(x0,x1);++x){
        if(Inside(x,z)&&!roads_[z][x]&&!IsWater(x,z)) zones_[z][x]=zone;
    }
}

void City::BulldozeAt(int x,int z){
    int bi=BuildingIndexAt(x,z); if(bi>=0){buildings_.erase(buildings_.begin()+bi);money_-=25;}
    else if(roads_[z][x]){roads_[z][x]=false;roadCount_=std::max(0,roadCount_-1);money_-=20;}
    else zones_[z][x]=Zone::None;
    RecalculateStats();
}

void City::Simulate(float dt){
    simAccumulator_+=dt; spawnAccumulator_+=dt;
    if(spawnAccumulator_>=0.22f){spawnAccumulator_=0.0f;TrySpawnBuilding();}
    if(simAccumulator_>=1.0f){
        simAccumulator_=0.0f; RecalculateStats(); RecalculateDemand(); money_+=population_*2+jobs_*3-roadCount_;
        if(!buildings_.empty()&&GetRandomValue(0,99)<16){
            Building& b=buildings_[GetRandomValue(0,(int)buildings_.size()-1)];
            float d=b.zone==Zone::Residential?residentialDemand_:(b.zone==Zone::Commercial?commercialDemand_:industrialDemand_);
            if(b.level<3&&d>0.55f){b.level++; if(b.zone==Zone::Residential)b.residents+=5;else b.jobs+=4;}
        }
    }
}

void City::TrySpawnBuilding(){
    for(int attempt=0;attempt<45;++attempt){
        int x=GetRandomValue(0,GRID_W-1),z=GetRandomValue(0,GRID_H-1); Zone zone=zones_[z][x];
        if(zone==Zone::None||roads_[z][x]||IsWater(x,z)||HasBuilding(x,z)||!AdjacentToRoad(x,z))continue;
        float demand=zone==Zone::Residential?residentialDemand_:(zone==Zone::Commercial?commercialDemand_:industrialDemand_);
        if(GetRandomValue(0,100)/100.0f>demand)continue;
        Building b; b.x=x;b.z=z;b.zone=zone;b.level=1;
        b.variant=GetRandomValue(0, zone==Zone::Residential?7:(zone==Zone::Commercial?5:5));
        if(zone==Zone::Residential)b.residents=GetRandomValue(5,14);
        else if(zone==Zone::Commercial)b.jobs=GetRandomValue(5,13);
        else b.jobs=GetRandomValue(8,18);
        buildings_.push_back(b); return;
    }
}

void City::RecalculateStats(){population_=jobs_=0;for(const auto&b:buildings_){population_+=b.residents;jobs_+=b.jobs;}}
void City::RecalculateDemand(){
    float hp=jobs_<=0?0.65f:Clamp((float)(jobs_+20)/(population_+30),0.15f,1.0f);
    float jp=population_<=0?0.35f:Clamp((float)(population_+20)/(jobs_+35),0.15f,1.0f);
    residentialDemand_=Clamp(0.30f+hp*0.65f,0.08f,1.0f);
    commercialDemand_=Clamp(0.20f+population_/240.0f-jobs_/950.0f,0.08f,0.95f);
    industrialDemand_=Clamp(0.22f+jp*0.55f,0.08f,0.95f);
}

void City::DrawTerrain() const {
    for(int z=0;z<GRID_H;++z)for(int x=0;x<GRID_W;++x){
        Vector3 p=GridToWorld(x,z); float h=p.y;
        if(h<WATER_LEVEL){
            DrawCube({p.x,WATER_LEVEL-0.08f,p.z},CELL_SIZE*1.01f,0.10f,CELL_SIZE*1.01f,Color{48,112,150,220});
        }else{
            float shade=Clamp((h+0.4f)/2.8f,0.0f,1.0f);
            Color grass{(unsigned char)(72+shade*25),(unsigned char)(111+shade*38),(unsigned char)(72+shade*20),255};
            DrawCube({p.x,h-0.10f,p.z},CELL_SIZE*1.01f,0.20f,CELL_SIZE*1.01f,grass);
            if(h>1.75f) DrawCube({p.x,h-0.045f,p.z},CELL_SIZE*0.98f,0.03f,CELL_SIZE*0.98f,Color{112,126,102,255});
        }
    }
}

void City::DrawRoads() const {
    for(int z=0;z<GRID_H;++z)for(int x=0;x<GRID_W;++x){
        if(!roads_[z][x])continue; Vector3 p=GridToWorld(x,z); p.y+=0.08f;
        DrawCube(p,CELL_SIZE*0.98f,0.10f,CELL_SIZE*0.98f,Color{62,65,69,255});
        bool hor=(Inside(x-1,z)&&roads_[z][x-1])||(Inside(x+1,z)&&roads_[z][x+1]);
        bool ver=(Inside(x,z-1)&&roads_[z-1][x])||(Inside(x,z+1)&&roads_[z+1][x]);
        if(hor)DrawCube({p.x,p.y+0.055f,p.z},CELL_SIZE*0.60f,0.014f,0.05f,Color{214,197,126,255});
        if(ver)DrawCube({p.x,p.y+0.055f,p.z},0.05f,0.014f,CELL_SIZE*0.60f,Color{214,197,126,255});
    }
}

void City::DrawWindows(Vector3 c,float w,float h,float d,int floors,int cols,bool frontBack) const {
    Color glass{92,145,170,255}; float fw=w/(cols+1); float fh=h/(floors+1);
    for(int f=1;f<=floors;++f)for(int col=1;col<=cols;++col){
        float x=c.x-w*0.5f+fw*col; float y=c.y-h*0.5f+fh*f;
        if(frontBack){DrawCube({x,y,c.z+d*0.505f},fw*0.42f,fh*0.34f,0.025f,glass);DrawCube({x,y,c.z-d*0.505f},fw*0.42f,fh*0.34f,0.025f,glass);}
        else {float z=c.z-d*0.5f+(d/(cols+1))*col;DrawCube({c.x+w*0.505f,y,z},0.025f,fh*0.34f,(d/(cols+1))*0.42f,glass);DrawCube({c.x-w*0.505f,y,z},0.025f,fh*0.34f,(d/(cols+1))*0.42f,glass);}
    }
}

void City::DrawResidential(const Building& b,Vector3 p) const {
    Color walls[]={{224,210,184,255},{200,218,205,255},{208,193,181,255},{189,204,223,255},{221,221,216,255},{185,207,185,255},{215,190,176,255},{197,190,218,255}};
    Color wall=walls[b.variant%8]; Color roof=Shift(wall,-55);
    float y=p.y+0.10f;
    if(b.variant<=2){
        float h=1.35f+0.20f*b.level; DrawCube({p.x,y+h*0.5f,p.z},1.45f,h,1.35f,wall);
        DrawCube({p.x,y+h+0.18f,p.z},1.58f,0.28f,1.48f,roof);
        DrawCube({p.x-0.42f,y+0.36f,p.z+0.69f},0.24f,0.62f,0.04f,Color{92,69,53,255});
        DrawWindows({p.x,y+h*0.5f,p.z},1.45f,h,1.35f,1+b.level/2,2,true);
        if(b.variant==2) DrawCube({p.x+0.52f,y+h+0.45f,p.z-0.35f},0.16f,0.55f,0.16f,Color{110,92,78,255});
    } else if(b.variant<=5){
        float h=1.9f+0.45f*b.level; DrawCube({p.x,y+h*0.5f,p.z},1.55f,h,1.50f,wall);
        DrawCube({p.x,y+h+0.12f,p.z},1.62f,0.20f,1.57f,roof);
        DrawWindows({p.x,y+h*0.5f,p.z},1.55f,h,1.50f,2+b.level,3,true);
        DrawWindows({p.x,y+h*0.5f,p.z},1.55f,h,1.50f,2+b.level,2,false);
        DrawCube({p.x-0.55f,y+0.38f,p.z+0.76f},0.28f,0.65f,0.04f,Color{85,66,52,255});
    } else {
        float h=2.6f+0.65f*b.level; DrawCube({p.x,y+h*0.5f,p.z},1.65f,h,1.62f,wall);
        DrawCube({p.x,y+h+0.10f,p.z},1.70f,0.16f,1.67f,Color{82,88,91,255});
        DrawWindows({p.x,y+h*0.5f,p.z},1.65f,h,1.62f,3+b.level,3,true);
        DrawWindows({p.x,y+h*0.5f,p.z},1.65f,h,1.62f,3+b.level,3,false);
        DrawCube({p.x,y+h+0.26f,p.z},0.56f,0.25f,0.50f,Color{156,160,157,255});
    }
}

void City::DrawCommercial(const Building& b,Vector3 p) const {
    Color walls[]={{192,198,201,255},{210,192,172,255},{174,195,207,255},{207,207,194,255},{174,184,174,255},{200,179,184,255}};
    Color wall=walls[b.variant%6]; float y=p.y+0.10f;
    if(b.variant<=2){
        float h=1.45f+0.25f*b.level; DrawCube({p.x,y+h*0.5f,p.z},1.70f,h,1.55f,wall);
        DrawCube({p.x,y+0.53f,p.z+0.79f},1.45f,0.56f,0.035f,Color{63,113,142,255});
        DrawCube({p.x,y+h+0.10f,p.z},1.76f,0.18f,1.61f,Color{72,78,82,255});
        DrawCube({p.x,y+h+0.34f,p.z},0.95f,0.24f,0.16f,Color{225,225,218,255});
    } else {
        float h=2.7f+0.65f*b.level; DrawCube({p.x,y+h*0.5f,p.z},1.70f,h,1.60f,wall);
        DrawWindows({p.x,y+h*0.5f,p.z},1.70f,h,1.60f,3+b.level,3,true);
        DrawWindows({p.x,y+h*0.5f,p.z},1.70f,h,1.60f,3+b.level,3,false);
        DrawCube({p.x,y+h+0.10f,p.z},1.74f,0.17f,1.64f,Color{65,70,74,255});
        if(b.variant==5) DrawCube({p.x,y+h+0.36f,p.z},0.66f,0.35f,0.66f,Color{141,147,148,255});
    }
}

void City::DrawIndustrial(const Building& b,Vector3 p) const {
    Color walls[]={{174,164,141,255},{157,168,174,255},{185,178,160,255},{143,153,149,255},{177,156,140,255},{161,168,149,255}};
    Color wall=walls[b.variant%6]; float y=p.y+0.10f;
    float h=1.35f+0.18f*b.level;
    DrawCube({p.x,y+h*0.5f,p.z},1.78f,h,1.68f,wall);
    DrawCube({p.x,y+h+0.09f,p.z},1.84f,0.16f,1.74f,Color{82,84,80,255});
    DrawCube({p.x-0.46f,y+0.48f,p.z+0.85f},0.52f,0.72f,0.04f,Color{74,78,79,255});
    if(b.variant%2==0) DrawCylinder({p.x+0.50f,y+h+0.62f,p.z-0.35f},0.13f,0.16f,1.20f,10,Color{112,105,93,255});
    if(b.variant>=3){
        DrawCylinder({p.x-0.45f,y+h+0.52f,p.z-0.38f},0.18f,0.18f,0.95f,12,Color{141,145,140,255});
        DrawCylinder({p.x+0.05f,y+h+0.42f,p.z-0.38f},0.15f,0.15f,0.76f,12,Color{137,143,145,255});
    }
    if(b.level>=2) DrawCube({p.x+0.58f,y+0.46f,p.z-0.40f},0.48f,0.72f,0.62f,Shift(wall,-18));
}

void City::DrawBuilding(const Building& b) const { Vector3 p=GridToWorld(b.x,b.z); if(b.zone==Zone::Residential)DrawResidential(b,p);else if(b.zone==Zone::Commercial)DrawCommercial(b,p);else DrawIndustrial(b,p); }
void City::DrawBuildings() const { for(const auto&b:buildings_)DrawBuilding(b); }

void City::Draw3D(const Camera3D& camera) const {
    (void)camera; DrawTerrain();
    for(int z=0;z<GRID_H;++z)for(int x=0;x<GRID_W;++x){
        if(zones_[z][x]!=Zone::None&&!roads_[z][x]&&!HasBuilding(x,z)&&!IsWater(x,z)){
            Vector3 p=GridToWorld(x,z); DrawCube({p.x,p.y+0.025f,p.z},CELL_SIZE*0.84f,0.035f,CELL_SIZE*0.84f,ZoneColor(zones_[z][x],80));
        }
    }
    DrawRoads(); DrawBuildings();
    if(hoverX_>=0&&hoverZ_>=0){Vector3 p=GridToWorld(hoverX_,hoverZ_);DrawCubeWires({p.x,p.y+0.16f,p.z},CELL_SIZE*0.94f,0.22f,CELL_SIZE*0.94f,tool_==Tool::Bulldoze?RED:YELLOW);}
}

void City::DrawUI() const {
    int w=GetScreenWidth();
    DrawRectangle(0,0,w,64,Color{18,21,23,245}); DrawText("CITY LAB v0.3",20,17,25,RAYWHITE);
    std::string money="CHF "+std::to_string(money_),pop="Einwohner  "+std::to_string(population_),jobs="Jobs  "+std::to_string(jobs_);
    DrawText(money.c_str(),210,20,18,Color{230,232,230,255});DrawText(pop.c_str(),360,20,18,Color{230,232,230,255});DrawText(jobs.c_str(),550,20,18,Color{230,232,230,255});
    DrawText(paused_?"PAUSIERT":"LAEUFT",w-115,20,17,paused_?Color{235,175,78,255}:Color{112,205,132,255});
    Rectangle panel{18,82,220,276};DrawRectangleRounded(panel,0.07f,8,Color{18,21,23,235});DrawText("BAUEN",34,98,19,RAYWHITE);
    const char* labels[5]={"1  Strasse","2  Wohnen","3  Gewerbe","4  Industrie","5  Bulldozer"};
    for(int i=0;i<5;++i){Rectangle r{31,130.0f+i*42,194,33};Color bg=((int)tool_==i)?Color{70,88,82,255}:Color{37,42,44,255};DrawRectangleRounded(r,0.15f,6,bg);DrawText(labels[i],43,(int)r.y+8,16,RAYWHITE);}
    DrawText("WASD bewegen",31,349,14,Color{190,195,194,255});DrawText("Rechtsziehen drehen  |  Mausrad zoom",31,370,14,Color{190,195,194,255});DrawText("SPACE pausieren",31,391,14,Color{190,195,194,255});
    Rectangle demand{(float)w-244,82,226,148};DrawRectangleRounded(demand,0.07f,8,Color{18,21,23,235});DrawText("NACHFRAGE",w-226,99,18,RAYWHITE);
    const char*dlabels[3]={"Wohnen","Gewerbe","Industrie"};float vals[3]={residentialDemand_,commercialDemand_,industrialDemand_};Color cols[3]={Color{82,190,110,255},Color{70,145,235,255},Color{225,180,65,255}};
    for(int i=0;i<3;++i){int y=132+i*30;DrawText(dlabels[i],w-226,y,14,Color{210,214,212,255});DrawRectangle(w-150,y+2,112,12,Color{45,48,50,255});DrawRectangle(w-150,y+2,(int)(112*vals[i]),12,cols[i]);}
    DrawText("Karte: Huegel + Wasser | 20 Gebaeudevarianten",w-430,GetScreenHeight()-28,14,Color{215,220,216,220});
}
