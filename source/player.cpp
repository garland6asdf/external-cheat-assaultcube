#include <windows.h>
#include <array>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "constants.h"
#include "additional_utils.h"
#include "my_memory.h"
#include "player.h"

using namespace std;

array<uint8_t, Constants::MAX_NECESSARY_OFFSET> PlayerData::get_buffer(HANDLE hprocess)
{
    array<uint8_t, Constants::MAX_NECESSARY_OFFSET> buffer{};
    if (ReadProcessMemory(hprocess, (LPCVOID)address, buffer.data(), Constants::MAX_NECESSARY_OFFSET, NULL))
    {
        return buffer;
    }
    else
    {
        throw runtime_error("couldn't read the process memory.");
    }
}

uint32_t PlayerData::get_players_quantity(HANDLE hprocess, uintptr_t module_base)
{
    uint32_t players_quantity{};
    if (ReadProcessMemory(hprocess, (LPCVOID)(module_base + Offsets::PLAYERS_COUNT), &players_quantity, sizeof(players_quantity), NULL))
    {
        return players_quantity;
    }
    else
    {
        throw runtime_error("couldn't read the process memory.");
    }
}

vector<uintptr_t> PlayerData::get_players_addresses(HANDLE hprocess, uintptr_t module_base)
{
    uint8_t players_quantity = get_players_quantity(hprocess, module_base);
    vector<uintptr_t> result;
    uintptr_t offsets[2] = {0x0, 0x0};
    for (uint8_t counter{}; counter < players_quantity - 1; counter++) // -1 из за локал плеера в списке
    {
        uintptr_t found_address = find_dynamic_addr((module_base + Offsets::ENTITY_LIST), offsets, 2, hprocess);
        result.push_back(found_address);
        offsets[0] += 0x4; // читаем некст плеера
    }
    return result;
}

void PlayerData::init(HANDLE hprocess, uintptr_t entered_address)
{
    address = entered_address;
    auto buffer = get_buffer(hprocess);
    if (*reinterpret_cast<int32_t *>(buffer.data()) != Constants::PLAYER_OBJ_VALUE)
    {
        throw invalid_argument("address is not valid.");
    }
    feet_pos = *reinterpret_cast<Vector3 *>(buffer.data() + Offsets::X_COORD);
    feet_pos.z -= 4.5f;
    head_pos = {feet_pos.x, feet_pos.y, feet_pos.z + 5.2f};
    hp = *reinterpret_cast<uint32_t *>(buffer.data() + Offsets::HEALTH);
    team_id = *reinterpret_cast<uint32_t *>(buffer.data() + Offsets::TEAM_ID);
}

bool PlayerData::is_enemy(HANDLE hprocess, uintptr_t local_player_address)
{
    if (team_id != Constants::UNDEFINED)
    {
        uint32_t local_player_team_id;
        if (ReadProcessMemory(hprocess, (LPCVOID)(local_player_address + Offsets::TEAM_ID), &local_player_team_id, sizeof(local_player_team_id), NULL))
        {
            return local_player_team_id != team_id;
        }
        else
        {
            throw runtime_error("couldn't read the process memory.");
        }
    }
    else
    {
        throw logic_error("initialize the player's parameters first.");
    }
}

bool PlayerData::is_alive()
{
    return (hp > 0 && hp <= 100);
}
