#include <cmath>
#include <windows.h>
#include "additional_utils.h"
#include "aimbot.h"

void CalcAngles(const Vector3 &local_pos, const Vector3 &target_pos, float &pitch, float &yaw)
{
    Vector3 delta = {
        target_pos.x - local_pos.x,
        target_pos.y - local_pos.y,
        target_pos.z - local_pos.z};

    float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

    yaw = atan2(delta.y, delta.x) * 180.0f / M_PI;

    pitch = -atan2(delta.z, hyp) * 180.0f / M_PI;

    if (yaw < 0)
        yaw += 360.0f;
    if (yaw > 360)
        yaw -= 360.0f;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

void aimbot(HANDLE hprocess, uintptr_t module_base){}
