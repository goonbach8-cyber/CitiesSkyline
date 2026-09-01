#pragma once
#include "GameTypes.h"

class BuildingRenderer {
public:
    static void DrawBuilding(const Building& b, Vector3 center, float lotWidth, float lotDepth);
    static void DrawService(const ServiceStructure& s, Vector3 center, float lotWidth, float lotDepth, float groundY);

private:
    static Color Shift(Color c, int amount);
    static void DrawWindows(Vector3 center, float width, float height, float depth, int floors, int columns, Color glass);
    static void DrawShadow(Vector3 center, float width, float depth, float groundY);
    static void DrawTree(Vector3 p, float scale, int seed);
    static void DrawResidential(const Building& b, Vector3 p, float lotW, float lotD);
    static void DrawCommercial(const Building& b, Vector3 p, float lotW, float lotD);
    static void DrawIndustrial(const Building& b, Vector3 p, float lotW, float lotD);
};
