#pragma once

#include <optional>
#include <windows.h>
#include "imgui.h"
#include "additional_utils.h"

using namespace std;

optional<Vector2> WorldToScreen(Vector3 worldPos, float *vm, int screenW, int screenH);
void espbox(HANDLE hprocess, uintptr_t module_base, uintptr_t local_player_address, ImDrawList *draw);