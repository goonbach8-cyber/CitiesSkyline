#pragma once

#include "raylib.h"

enum class Tool {
    Road = 0,
    Residential = 1,
    Commercial = 2,
    Industrial = 3,
    Bulldoze = 4,
    Power = 5,
    Water = 6,
    Park = 7
};

enum class Zone {
    None = 0,
    Residential,
    Commercial,
    Industrial
};

enum class ServiceKind {
    PowerPlant,
    WaterTower,
    Park
};

struct Building {
    int x = 0;
    int z = 0;
    int w = 1;
    int d = 1;
    int orientation = 0; // legacy cardinal orientation
    float rotationDegrees = 0.0f; // visual orientation towards the curved road

    Zone zone = Zone::None;
    int level = 1;
    int variant = 0;
    int palette = 0;
    int roofStyle = 0;
    int detailSeed = 0;
    int ageDays = 0;

    int capacity = 0;   // residential capacity
    int occupants = 0; // residential occupants
    int jobs = 0;       // commercial/industrial jobs
    int employees = 0;

    bool powered = false;
    bool watered = false;
    float landValue = 0.45f;
    float happiness = 0.55f;
};

struct ServiceStructure {
    ServiceKind kind = ServiceKind::PowerPlant;
    int x = 0;
    int z = 0;
    int w = 1;
    int d = 1;
    float radius = 18.0f;
    int capacity = 0;
    int maintenance = 0;
};
