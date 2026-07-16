#include <cstdint>
#include <optional>
#include <iostream>
#include <windows.h>
#include <vector>
#include "imgui.h"
#include "additional_utils.h"
#include "constants.h"
#include "player.h"
#include "esp.h"

using namespace std;

optional<Vector2> WorldToScreen(Vector3 worldPos, float *vm, int screenW, int screenH)
{
    float clipX = worldPos.x * vm[0] + worldPos.y * vm[4] + worldPos.z * vm[8] + vm[12];
    float clipY = worldPos.x * vm[1] + worldPos.y * vm[5] + worldPos.z * vm[9] + vm[13];
    float clipW = worldPos.x * vm[3] + worldPos.y * vm[7] + worldPos.z * vm[11] + vm[15];
    if (clipW < 0.1f)
        return nullopt;
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;
    Vector2 screenPos;
    screenPos.x = screenW / 2 + ndcX * screenW / 2;
    screenPos.y = screenH / 2 - ndcY * screenH / 2;
    return screenPos;
}

void espbox(HANDLE hprocess, uintptr_t module_base, uintptr_t local_player_address, ImDrawList *draw){
    static uint8_t counter{};
    static vector<uintptr_t> players_list;
    try{
        if (counter % TimeDelay::RESCAN_PLAYERS_ADDR_INTERVAL == 0){
            players_list = PlayerData::get_players_addresses(hprocess, module_base);
        }
    }
    catch (...){
        cout << "fatal error in func espbox." << endl;
    }
    float view_matrix[16];
    if(!ReadProcessMemory(hprocess, (LPCVOID)Constants::VIEW_MATRIX, &view_matrix, sizeof(view_matrix), NULL)){
        cout << "vm is cant reading." << endl;
        cout << "fatal error in func espbox." << endl;
    }
    for(uintptr_t player_address : players_list){
        try{
            PlayerData player;
            player.init(hprocess, player_address);
            auto feet_screen = WorldToScreen(player.get_feet_pos(), view_matrix, 1920, 1080);
            auto head_screen = WorldToScreen(player.get_head_pos(), view_matrix, 1920, 1080);
            if(player.is_alive() && player.is_enemy(hprocess, local_player_address)
                && feet_screen
                && head_screen
            ){
                float height = feet_screen->y - head_screen->y;
                float width = height * 0.35f;
                draw->AddRect(
                    ImVec2(feet_screen->x - width / 2, head_screen->y),
                    ImVec2(feet_screen->x + width / 2, feet_screen->y),
                    IM_COL32(255, 0, 0, 255), 2.0f, 0, 3.0f
                );
            }
        }
        catch(...){
            cout << "skipped player." << endl;
        }
    }
    counter++;
}