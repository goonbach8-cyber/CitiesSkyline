#include "CameraController.h"
#include "raymath.h"
#include <cmath>
CameraController::CameraController(){camera_.up={0,1,0};camera_.fovy=45;camera_.projection=CAMERA_PERSPECTIVE;previousMouse_=GetMousePosition();RebuildCamera();}
void CameraController::Update(float dt){Vector2 mouse=GetMousePosition();Vector2 delta={mouse.x-previousMouse_.x,mouse.y-previousMouse_.y};previousMouse_=mouse;
if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){yaw_-=delta.x*0.32f;pitch_+=delta.y*0.22f;pitch_=Clamp(pitch_,24.0f,78.0f);}if(IsKeyDown(KEY_Q))yaw_-=70*dt;if(IsKeyDown(KEY_E))yaw_+=70*dt;float wheel=GetMouseWheelMove();if(wheel!=0)distance_=Clamp(distance_-wheel*4.5f,11.0f,120.0f);
float yawRad=DEG2RAD*yaw_;Vector3 forward={sinf(yawRad),0,cosf(yawRad)};Vector3 right={cosf(yawRad),0,-sinf(yawRad)};float speed=27.0f*dt*(distance_/45.0f);if(IsKeyDown(KEY_W))target_=Vector3Add(target_,Vector3Scale(forward,-speed));if(IsKeyDown(KEY_S))target_=Vector3Add(target_,Vector3Scale(forward,speed));if(IsKeyDown(KEY_A))target_=Vector3Add(target_,Vector3Scale(right,-speed));if(IsKeyDown(KEY_D))target_=Vector3Add(target_,Vector3Scale(right,speed));target_.x=Clamp(target_.x,-75.0f,75.0f);target_.z=Clamp(target_.z,-75.0f,75.0f);RebuildCamera();}
void CameraController::RebuildCamera(){float yaw=DEG2RAD*yaw_,pitch=DEG2RAD*pitch_;Vector3 offset={distance_*cosf(pitch)*sinf(yaw),distance_*sinf(pitch),distance_*cosf(pitch)*cosf(yaw)};camera_.position=Vector3Add(target_,offset);camera_.target=target_;}
