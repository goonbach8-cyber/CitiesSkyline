#pragma once
#include "raylib.h"

class CameraController {
public:
    CameraController();
    void Update(float dt);
    const Camera3D& GetCamera() const { return camera_; }

private:
    void RebuildCamera();

    Camera3D camera_{};
    Vector3 target_{0.0f, 0.45f, 0.0f};
    float yaw_ = 45.0f;
    float pitch_ = 52.0f;
    float distance_ = 46.0f;
    Vector2 previousMouse_{};
};
