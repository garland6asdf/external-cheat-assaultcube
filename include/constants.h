#pragma once

#include <windows.h>
#include <cstdint>

namespace Offsets
{   
    constexpr uintptr_t LOCAL_PLAYER = 0x17F110;
    constexpr uintptr_t ENTITY_LIST = 0x191FCC;
    constexpr uintptr_t TEAM_ID = 0x30C;
    constexpr uintptr_t HEALTH = 0xEC;
    constexpr uintptr_t X_COORD = 0x4;
    constexpr uintptr_t PLAYERS_COUNT = 0x18AC0C;
}

namespace TimeDelay
{
    constexpr int RESCAN_PLAYERS_ADDR_INTERVAL{30};
}

namespace Constants
{
    constexpr unsigned int PLAYER_OBJ_VALUE{5558396};
    constexpr uintptr_t VIEW_MATRIX = 0x57DFD0;
    constexpr uintptr_t MAX_NECESSARY_OFFSET = Offsets::TEAM_ID + sizeof(int32_t);
    constexpr uint32_t UNDEFINED{666666};
}
