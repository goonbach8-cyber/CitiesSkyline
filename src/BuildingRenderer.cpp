#include "BuildingRenderer.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Color BuildingRenderer::Shift(Color c, int amount) {
    auto cc = [](int v) { return (unsigned char)std::max(0, std::min(255, v)); };
    return Color{cc(c.r + amount), cc(c.g + amount), cc(c.b + amount), c.a};
}

void BuildingRenderer::DrawShadow(Vector3 center, float width, float depth, float groundY) {
    DrawCube({center.x + 0.18f, groundY + 0.015f, center.z + 0.18f}, width * 0.95f, 0.02f, depth * 0.95f, Color{35, 43, 40, 80});
}

void BuildingRenderer::DrawWindows(Vector3 c, float w, float h, float d, int floors, int columns, Color glass) {
    if (floors < 1 || columns < 1) return;
    float floorStep = h / (floors + 1.0f);
    float colStepW = w / (columns + 1.0f);
    float colStepD = d / (columns + 1.0f);
    for (int f = 1; f <= floors; ++f) {
        float y = c.y - h * 0.5f + floorStep * f;
        for (int col = 1; col <= columns; ++col) {
            float x = c.x - w * 0.5f + colStepW * col;
            float z = c.z - d * 0.5f + colStepD * col;
            DrawCube({x, y, c.z + d * 0.505f}, colStepW * 0.42f, floorStep * 0.33f, 0.028f, glass);
            DrawCube({x, y, c.z - d * 0.505f}, colStepW * 0.42f, floorStep * 0.33f, 0.028f, glass);
            DrawCube({c.x + w * 0.505f, y, z}, 0.028f, floorStep * 0.33f, colStepD * 0.42f, glass);
            DrawCube({c.x - w * 0.505f, y, z}, 0.028f, floorStep * 0.33f, colStepD * 0.42f, glass);
        }
    }
}

void BuildingRenderer::DrawTree(Vector3 p, float s, int seed) {
    Color trunk{92, 70, 49, 255};
    Color foliage = (seed % 3 == 0) ? Color{72, 118, 70, 255} : ((seed % 3 == 1) ? Color{65, 108, 62, 255} : Color{83, 128, 72, 255});
    DrawCylinder({p.x, p.y + 0.34f * s, p.z}, 0.07f * s, 0.09f * s, 0.68f * s, 6, trunk);
    DrawSphereEx({p.x, p.y + 0.92f * s, p.z}, 0.40f * s, 4, 7, foliage);
    DrawSphereEx({p.x + 0.17f * s, p.y + 1.02f * s, p.z - 0.08f * s}, 0.26f * s, 3, 6, Shift(foliage, 10));
}

void BuildingRenderer::DrawSimplified(const Building& b, Vector3 p, float lotW, float lotD) {
    float ground = p.y;
    if (b.zone == Zone::Residential) {
        static const Color walls[] = {
            {224,211,188,255},{202,219,206,255},{214,194,181,255},{190,207,225,255},
            {224,224,218,255},{183,208,188,255},{220,190,174,255},{202,192,219,255}
        };
        Color wall = walls[b.palette % 8];
        Color roof = Shift(wall, -65);
        float bw = lotW * 0.76f;
        float bd = lotD * 0.74f;
        float h = (b.level >= 3 ? 3.4f : (b.level == 2 ? 2.35f : 1.35f)) + (b.variant % 3) * 0.14f;
        DrawCube({p.x, ground + h * 0.5f + 0.06f, p.z}, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.04f, 0.20f + (b.variant % 2) * 0.08f, bd * 1.04f, roof);
        if (b.level >= 3 && (b.variant % 2 == 0)) {
            DrawCube({p.x + bw * 0.22f, ground + h + 0.27f, p.z}, bw * 0.28f, 0.18f, bd * 0.24f, Shift(roof, 18));
        }
    } else if (b.zone == Zone::Commercial) {
        static const Color walls[] = {
            {198,203,205,255},{214,195,174,255},{176,198,211,255},{212,211,197,255},
            {177,188,177,255},{207,184,189,255},{196,205,217,255},{220,204,180,255}
        };
        Color wall = walls[b.palette % 8];
        float bw = lotW * 0.83f;
        float bd = lotD * 0.80f;
        float h = 1.55f + (b.variant % 4) * 0.25f + (b.level - 1) * 0.75f;
        DrawCube({p.x, ground + h * 0.5f + 0.06f, p.z}, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.03f, 0.18f, bd * 1.03f, Shift(wall, -65));
        if (b.variant % 3 == 0) DrawCube({p.x, ground + 0.64f, p.z + bd * 0.51f}, bw * 0.62f, 0.52f, 0.03f, Color{66,126,158,255});
    } else {
        static const Color walls[] = {
            {176,166,142,255},{158,169,176,255},{188,180,160,255},{145,155,151,255},
            {181,158,141,255},{163,170,150,255},{173,178,183,255},{191,169,145,255}
        };
        Color wall = walls[b.palette % 8];
        float bw = lotW * 0.86f;
        float bd = lotD * 0.82f;
        float h = 1.35f + (b.variant % 3) * 0.18f + (b.level - 1) * 0.22f;
        DrawCube({p.x, ground + h * 0.5f + 0.06f, p.z}, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.09f, p.z}, bw * 1.03f, 0.16f, bd * 1.03f, Shift(wall, -60));
        if (b.variant % 4 == 0) {
            DrawCylinder({p.x + bw * 0.28f, ground + h + 0.48f, p.z - bd * 0.22f}, 0.10f, 0.13f, 0.82f, 6, Color{108,107,101,255});
        }
    }
}

void BuildingRenderer::DrawResidential(const Building& b, Vector3 p, float lotW, float lotD) {
    static const Color palettes[] = {
        {224, 211, 188, 255}, {202, 219, 206, 255}, {214, 194, 181, 255}, {190, 207, 225, 255},
        {224, 224, 218, 255}, {183, 208, 188, 255}, {220, 190, 174, 255}, {202, 192, 219, 255},
        {229, 215, 205, 255}, {190, 199, 191, 255}, {215, 222, 230, 255}, {205, 186, 165, 255}
    };
    Color wall = palettes[b.palette % 12];
    Color trim = Shift(wall, 20);
    Color roof = Shift(wall, -70);
    Color glass{95, 145, 174, 255};
    float ground = p.y;
    float bw = lotW * 0.72f;
    float bd = lotD * 0.70f;

    DrawShadow(p, bw, bd, ground);

    int style = b.variant % 12;
    if (b.level == 1 && style <= 7) {
        float h = 1.25f + 0.08f * (style % 3);
        Vector3 body{p.x, ground + h * 0.5f + 0.09f, p.z};
        if (style == 4 || style == 5) {
            bw *= 0.86f;
            bd *= 0.94f;
        }
        DrawCube(body, bw, h, bd, wall);
        DrawCube({p.x, ground + 0.34f, p.z + bd * 0.505f}, bw * 0.22f, 0.58f, 0.035f, Color{99, 73, 53, 255});
        DrawWindows(body, bw, h, bd, 1, (bw > 2.3f ? 3 : 2), glass);

        if (b.roofStyle % 3 == 0 && fabsf(bw - bd) < 0.9f) {
            DrawCylinder({p.x, ground + h + 0.12f, p.z}, 0.10f, std::max(bw, bd) * 0.66f, 0.70f, 4, roof);
        } else {
            DrawCube({p.x, ground + h + 0.18f, p.z}, bw * 1.08f, 0.28f, bd * 1.08f, roof);
            if (b.roofStyle % 3 == 2) DrawCube({p.x, ground + h + 0.36f, p.z}, bw * 0.54f, 0.13f, bd * 0.54f, Shift(roof, 18));
        }

        if (style % 2 == 0) {
            DrawCube({p.x + bw * 0.36f, ground + 0.28f, p.z - bd * 0.34f}, bw * 0.25f, 0.48f, bd * 0.28f, Shift(wall, -12));
            DrawCube({p.x + bw * 0.36f, ground + 0.29f, p.z - bd * 0.49f}, bw * 0.20f, 0.30f, 0.035f, Color{80, 84, 82, 255});
        }
        if (style == 2 || style == 6) {
            DrawCube({p.x - bw * 0.37f, ground + h + 0.42f, p.z - bd * 0.22f}, 0.16f, 0.62f, 0.16f, Color{111, 92, 78, 255});
        }
        if (lotW > 2.5f && (b.detailSeed % 2 == 0)) {
            DrawTree({p.x - lotW * 0.35f, ground, p.z + lotD * 0.28f}, 0.82f, b.detailSeed);
        }
        if (b.detailSeed % 3 == 0) {
            DrawCube({p.x, ground + 0.045f, p.z + lotD * 0.40f}, lotW * 0.44f, 0.05f, 0.30f, Color{181, 173, 151, 255});
        }
    } else if (b.level <= 2 || style <= 4) {
        float h = 2.35f + 0.22f * (style % 4) + (b.level - 1) * 0.42f;
        bw = lotW * 0.78f;
        bd = lotD * 0.76f;
        Vector3 body{p.x, ground + h * 0.5f + 0.08f, p.z};
        DrawCube(body, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.03f, 0.18f, bd * 1.03f, roof);
        DrawWindows(body, bw, h, bd, 2 + (b.level > 2 ? 1 : 0), std::max(2, b.w + 1), glass);

        if (style % 3 == 0) {
            for (int f = 0; f < 2; ++f) {
                DrawCube({p.x + bw * 0.50f, ground + 0.72f + f * 0.85f, p.z}, 0.16f, 0.08f, bd * 0.58f, trim);
            }
        }
        DrawCube({p.x - bw * 0.33f, ground + 0.38f, p.z + bd * 0.51f}, 0.30f, 0.66f, 0.035f, Color{90, 67, 51, 255});
        if (b.detailSeed % 2 == 0) {
            DrawTree({p.x + lotW * 0.38f, ground, p.z - lotD * 0.34f}, 0.72f, b.detailSeed + 5);
        }
    } else {
        float floors = 4.0f + (float)((style + b.level) % 4);
        float h = floors * 0.66f;
        bw = lotW * 0.82f;
        bd = lotD * 0.80f;
        Vector3 body{p.x, ground + h * 0.5f + 0.08f, p.z};
        DrawCube(body, bw, h, bd, wall);
        DrawWindows(body, bw, h, bd, (int)floors, std::max(3, b.w + 2), glass);
        DrawCube({p.x, ground + h + 0.11f, p.z}, bw * 1.02f, 0.17f, bd * 1.02f, Color{78, 84, 88, 255});
        DrawCube({p.x + bw * 0.18f, ground + h + 0.32f, p.z - bd * 0.15f}, bw * 0.30f, 0.24f, bd * 0.24f, Color{151, 157, 159, 255});
        if (style % 2 == 0) {
            for (int f = 1; f <= (int)floors - 1; ++f) {
                DrawCube({p.x - bw * 0.505f, ground + f * 0.66f, p.z}, 0.10f, 0.07f, bd * 0.52f, trim);
            }
        }
        if (lotW > 3.6f) {
            DrawTree({p.x - lotW * 0.40f, ground, p.z + lotD * 0.36f}, 0.76f, b.detailSeed + 7);
            DrawTree({p.x + lotW * 0.40f, ground, p.z + lotD * 0.34f}, 0.70f, b.detailSeed + 8);
        }
    }
}

void BuildingRenderer::DrawCommercial(const Building& b, Vector3 p, float lotW, float lotD) {
    static const Color palettes[] = {
        {198, 203, 205, 255}, {214, 195, 174, 255}, {176, 198, 211, 255}, {212, 211, 197, 255},
        {177, 188, 177, 255}, {207, 184, 189, 255}, {196, 205, 217, 255}, {220, 204, 180, 255},
        {182, 192, 202, 255}, {206, 206, 206, 255}
    };
    Color wall = palettes[b.palette % 10];
    Color dark = Shift(wall, -72);
    Color glass{66, 126, 158, 255};
    float ground = p.y;
    float bw = lotW * 0.82f;
    float bd = lotD * 0.80f;
    int style = b.variant % 10;
    DrawShadow(p, bw, bd, ground);

    if (style <= 3 && b.level <= 2) {
        float h = 1.42f + 0.20f * (style % 3) + 0.20f * (b.level - 1);
        Vector3 body{p.x, ground + h * 0.5f + 0.08f, p.z};
        DrawCube(body, bw, h, bd, wall);
        DrawCube({p.x, ground + 0.54f, p.z + bd * 0.505f}, bw * 0.78f, 0.62f, 0.04f, glass);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.04f, 0.18f, bd * 1.04f, dark);
        Color awning = style % 2 == 0 ? Color{177, 72, 62, 255} : Color{58, 126, 107, 255};
        DrawCube({p.x, ground + 0.92f, p.z + bd * 0.56f}, bw * 0.88f, 0.10f, 0.26f, awning);
        DrawCube({p.x, ground + h + 0.35f, p.z + bd * 0.20f}, bw * 0.55f, 0.30f, 0.13f, Color{235, 232, 216, 255});
        if (style == 3 && lotW > 3.6f) {
            DrawCube({p.x - bw * 0.25f, ground + 0.14f, p.z - bd * 0.52f}, bw * 0.42f, 0.16f, 0.58f, Color{92, 94, 92, 255});
        }
    } else if (style <= 6) {
        float floors = 3.0f + (float)(b.level + style % 3);
        float h = floors * 0.74f;
        Vector3 body{p.x, ground + h * 0.5f + 0.08f, p.z};
        DrawCube(body, bw, h, bd, wall);
        DrawWindows(body, bw, h, bd, (int)floors, std::max(3, b.w + 2), glass);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.03f, 0.18f, bd * 1.03f, dark);
        DrawCube({p.x, ground + 0.65f, p.z + bd * 0.515f}, bw * 0.64f, 0.76f, 0.04f, Color{56, 108, 134, 255});
        if (style % 2 == 0) {
            DrawCube({p.x + bw * 0.22f, ground + h + 0.34f, p.z}, bw * 0.28f, 0.26f, bd * 0.25f, Color{145, 152, 154, 255});
        }
    } else {
        float h = 1.50f + 0.22f * b.level;
        DrawCube({p.x, ground + h * 0.5f + 0.08f, p.z}, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.10f, p.z}, bw * 1.03f, 0.18f, bd * 1.03f, dark);
        if (style == 7) {
            DrawCube({p.x, ground + 0.78f, p.z + bd * 0.51f}, bw * 0.86f, 0.78f, 0.04f, glass);
            DrawCube({p.x, ground + 0.34f, p.z - bd * 0.52f}, bw * 0.24f, 0.50f, 0.38f, Color{94, 97, 94, 255});
        } else if (style == 8) {
            DrawCube({p.x - bw * 0.20f, ground + 0.52f, p.z + bd * 0.51f}, bw * 0.46f, 0.62f, 0.04f, glass);
            DrawCube({p.x + bw * 0.30f, ground + 0.52f, p.z + bd * 0.51f}, bw * 0.28f, 0.62f, 0.04f, glass);
            DrawCube({p.x + bw * 0.34f, ground + h + 0.38f, p.z}, 0.14f, 0.58f, 0.14f, Color{80, 84, 82, 255});
        } else {
            DrawCube({p.x, ground + 0.86f, p.z}, bw * 0.48f, h * 0.82f, bd * 0.45f, Shift(wall, 18));
            DrawWindows({p.x, ground + h * 0.58f, p.z}, bw * 0.48f, h * 0.72f, bd * 0.45f, 2, 2, glass);
        }
    }
}

void BuildingRenderer::DrawIndustrial(const Building& b, Vector3 p, float lotW, float lotD) {
    static const Color palettes[] = {
        {176, 166, 142, 255}, {158, 169, 176, 255}, {188, 180, 160, 255}, {145, 155, 151, 255},
        {181, 158, 141, 255}, {163, 170, 150, 255}, {173, 178, 183, 255}, {191, 169, 145, 255},
        {155, 161, 166, 255}, {184, 184, 169, 255}
    };
    Color wall = palettes[b.palette % 10];
    Color roof = Shift(wall, -66);
    float ground = p.y;
    float bw = lotW * 0.86f;
    float bd = lotD * 0.84f;
    int style = b.variant % 10;
    DrawShadow(p, bw, bd, ground);

    if (style <= 3) {
        float h = 1.50f + 0.18f * b.level;
        DrawCube({p.x, ground + h * 0.5f + 0.08f, p.z}, bw, h, bd, wall);
        DrawCube({p.x, ground + h + 0.09f, p.z}, bw * 1.03f, 0.16f, bd * 1.03f, roof);
        for (int i = -1; i <= 1; ++i) {
            DrawCube({p.x + i * bw * 0.27f, ground + 0.49f, p.z + bd * 0.505f}, bw * 0.19f, 0.72f, 0.035f, Color{77, 82, 84, 255});
        }
        if (style % 2 == 0) DrawCylinder({p.x + bw * 0.30f, ground + h + 0.64f, p.z - bd * 0.25f}, 0.13f, 0.16f, 1.18f, 10, Color{111, 106, 96, 255});
        if (style == 3) {
            DrawCube({p.x - bw * 0.30f, ground + h + 0.30f, p.z}, bw * 0.25f, 0.40f, bd * 0.24f, Shift(wall, -18));
        }
    } else if (style <= 6) {
        float h = 1.26f + 0.16f * b.level;
        DrawCube({p.x - bw * 0.12f, ground + h * 0.5f + 0.08f, p.z}, bw * 0.74f, h, bd, wall);
        DrawCube({p.x - bw * 0.12f, ground + h + 0.09f, p.z}, bw * 0.77f, 0.15f, bd * 1.03f, roof);
        DrawCylinder({p.x + bw * 0.34f, ground + 0.72f, p.z - bd * 0.22f}, 0.30f, 0.30f, 1.30f, 14, Color{145, 149, 146, 255});
        DrawCylinder({p.x + bw * 0.34f, ground + 1.42f, p.z - bd * 0.22f}, 0.08f, 0.31f, 0.18f, 14, Color{126, 132, 130, 255});
        if (style >= 5) {
            DrawCylinder({p.x + bw * 0.34f, ground + 0.60f, p.z + bd * 0.25f}, 0.24f, 0.24f, 1.10f, 12, Color{132, 139, 140, 255});
        }
        if (style == 6) {
            DrawCylinder({p.x - bw * 0.36f, ground + h + 0.78f, p.z - bd * 0.32f}, 0.11f, 0.14f, 1.40f, 10, Color{103, 101, 95, 255});
        }
    } else {
        float hallH = 1.18f + 0.12f * b.level;
        DrawCube({p.x, ground + hallH * 0.5f + 0.08f, p.z}, bw, hallH, bd * 0.72f, wall);
        DrawCube({p.x, ground + hallH + 0.09f, p.z}, bw * 1.03f, 0.15f, bd * 0.75f, roof);
        DrawCube({p.x, ground + 0.30f, p.z + bd * 0.40f}, bw * 0.90f, 0.16f, bd * 0.20f, Color{105, 106, 101, 255});
        for (int i = -2; i <= 2; ++i) {
            DrawCube({p.x + i * bw * 0.18f, ground + 0.47f, p.z + bd * 0.47f}, bw * 0.12f, 0.60f, 0.04f, Color{76, 80, 82, 255});
        }
        if (style == 9) {
            DrawCylinder({p.x - bw * 0.28f, ground + hallH + 0.78f, p.z - bd * 0.22f}, 0.11f, 0.14f, 1.40f, 10, Color{99, 97, 92, 255});
            DrawCylinder({p.x + bw * 0.27f, ground + hallH + 0.66f, p.z - bd * 0.26f}, 0.10f, 0.13f, 1.18f, 10, Color{103, 101, 95, 255});
        }
    }
}

void BuildingRenderer::DrawBuilding(const Building& b, Vector3 center, float lotWidth, float lotDepth, bool detailed) {
    if (!detailed) {
        DrawSimplified(b, center, lotWidth, lotDepth);
        return;
    }

    if (b.zone == Zone::Residential) DrawResidential(b, center, lotWidth, lotDepth);
    else if (b.zone == Zone::Commercial) DrawCommercial(b, center, lotWidth, lotDepth);
    else if (b.zone == Zone::Industrial) DrawIndustrial(b, center, lotWidth, lotDepth);

    // Rounded annexes/towers break up the box-only silhouette while keeping the
    // procedural system lightweight enough for WebGL.
    if (b.zone == Zone::Residential && b.level >= 2 && (b.variant % 4 == 3)) {
        float h = 1.8f + b.level * 0.75f;
        float r = std::min(lotWidth, lotDepth) * 0.17f;
        Vector3 q{center.x + lotWidth * 0.23f, center.y + 0.08f, center.z - lotDepth * 0.18f};
        DrawCylinder({q.x, q.y, q.z}, r, r, h, 18, Color{203, 210, 207, 255});
        for (int f = 1; f < b.level + 2; ++f) {
            DrawCylinder({q.x, q.y + f * h / (b.level + 2.0f), q.z}, r * 1.015f, r * 1.015f, 0.045f, 18, Color{91, 142, 166, 255});
        }
    } else if (b.zone == Zone::Commercial && b.level >= 2 && (b.variant % 3 == 1)) {
        float h = 2.4f + b.level * 0.95f;
        float r = std::min(lotWidth, lotDepth) * 0.20f;
        Vector3 q{center.x - lotWidth * 0.18f, center.y + 0.08f, center.z};
        DrawCylinder({q.x, q.y, q.z}, r * 0.90f, r, h, 20, Color{105, 151, 171, 255});
        DrawCylinder({q.x, q.y + h, q.z}, r * 0.72f, r * 0.90f, 0.22f, 20, Color{66, 83, 92, 255});
    }

    if (!b.powered) {
        DrawCylinder({center.x, center.y + 0.22f, center.z}, 0.05f, 0.05f, 0.42f, 6, Color{232, 183, 54, 255});
    }
    if (!b.watered) {
        DrawCylinder({center.x + 0.13f, center.y + 0.22f, center.z}, 0.05f, 0.05f, 0.42f, 6, Color{67, 151, 222, 255});
    }
}

void BuildingRenderer::DrawService(const ServiceStructure& s, Vector3 p, float lotW, float lotD, float groundY) {
    DrawShadow(p, lotW * 0.84f, lotD * 0.84f, groundY);
    if (s.kind == ServiceKind::PowerPlant) {
        Color wall{167, 171, 166, 255};
        Color roof{74, 78, 76, 255};
        DrawCube({p.x, groundY + 0.72f, p.z}, lotW * 0.76f, 1.34f, lotD * 0.72f, wall);
        DrawCube({p.x, groundY + 1.43f, p.z}, lotW * 0.80f, 0.15f, lotD * 0.76f, roof);
        DrawCylinder({p.x + lotW * 0.22f, groundY + 2.10f, p.z - lotD * 0.18f}, 0.17f, 0.22f, 1.50f, 12, Color{108, 107, 101, 255});
        DrawCylinder({p.x - lotW * 0.18f, groundY + 1.95f, p.z - lotD * 0.22f}, 0.14f, 0.19f, 1.20f, 12, Color{113, 111, 104, 255});
        DrawCube({p.x, groundY + 0.52f, p.z + lotD * 0.37f}, lotW * 0.58f, 0.62f, 0.04f, Color{65, 83, 92, 255});
    } else if (s.kind == ServiceKind::WaterTower) {
        Color metal{154, 165, 167, 255};
        float top = groundY + 2.35f;
        for (int sx : {-1, 1}) for (int sz : {-1, 1}) {
            DrawCylinder({p.x + sx * 0.28f, groundY + 1.00f, p.z + sz * 0.28f}, 0.055f, 0.075f, 1.95f, 6, Color{103, 111, 112, 255});
        }
        DrawCylinder({p.x, top, p.z}, 0.58f, 0.72f, 0.88f, 16, metal);
        DrawCylinder({p.x, top + 0.50f, p.z}, 0.06f, 0.56f, 0.22f, 16, Shift(metal, -12));
    } else {
        Color path{190, 180, 153, 255};
        Color grass{78, 139, 78, 255};
        float radius = std::min(lotW, lotD) * 0.40f;
        DrawCylinder({p.x, groundY + 0.015f, p.z}, radius, radius, 0.055f, 24, grass);
        DrawCylinder({p.x, groundY + 0.055f, p.z}, radius * 0.52f, radius * 0.52f, 0.035f, 24, path);
        DrawCylinder({p.x, groundY + 0.080f, p.z}, radius * 0.30f, radius * 0.30f, 0.045f, 24, grass);
        DrawTree({p.x - lotW * 0.27f, groundY, p.z - lotD * 0.25f}, 0.92f, s.x * 13 + s.z * 7);
        DrawTree({p.x + lotW * 0.29f, groundY, p.z + lotD * 0.24f}, 0.82f, s.x * 17 + s.z * 11);
        DrawTree({p.x + lotW * 0.25f, groundY, p.z - lotD * 0.27f}, 0.72f, s.x * 5 + s.z * 19);
        DrawCylinder({p.x, groundY + 0.12f, p.z}, 0.18f, 0.22f, 0.24f, 18, Color{128, 142, 145, 255});
        DrawSphereEx({p.x, groundY + 0.31f, p.z}, 0.13f, 4, 8, Color{78, 164, 205, 210});
    }
}
