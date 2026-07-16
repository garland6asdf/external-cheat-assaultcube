#pragma once

#include <cstdint>
#include <windows.h>
#include <string>

using namespace std;

uintptr_t find_dynamic_addr(
    uintptr_t base_plus_static_offset,
    uintptr_t *offsets,
    int length,
    HANDLE hprocess
);
uintptr_t GetModuleBase(HANDLE hProcess, const wstring &modName);
DWORD GetPID(const wstring &processName);
