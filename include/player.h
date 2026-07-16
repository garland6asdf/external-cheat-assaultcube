#pragma once

#include <windows.h>
#include <array>
#include <cstdint>
#include <vector>

using namespace std;

class PlayerData
{
    uintptr_t address{Constants::UNDEFINED};
    Vector3 feet_pos{Constants::UNDEFINED}, head_pos{Constants::UNDEFINED};
    uint32_t hp{Constants::UNDEFINED}, team_id{Constants::UNDEFINED};
    
    array<uint8_t, Constants::MAX_NECESSARY_OFFSET> get_buffer(HANDLE hprocess);

    static uint32_t get_players_quantity(HANDLE hprocess, uintptr_t module_base);


public:

    static vector<uintptr_t> get_players_addresses(HANDLE hprocess, uintptr_t module_base);

    void init(HANDLE hprocess, uintptr_t entered_address);

    bool is_enemy(HANDLE hprocess, uintptr_t local_player_address);

    bool is_alive();

    inline Vector3 get_feet_pos() const { return feet_pos; }
    inline Vector3 get_head_pos() const { return head_pos; }
};