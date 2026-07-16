#pragma once

#include <windows.h>
#include "additional_utils.h"

void CalcAngles(const Vector3 &local_pos, const Vector3 &target_pos, float &pitch, float &yaw);
void aimbot(HANDLE hprocess, uintptr_t module_base);