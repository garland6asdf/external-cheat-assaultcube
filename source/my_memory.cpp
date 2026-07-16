#include <cstdint>
#include <windows.h>
#include <string>
#include <psapi.h>
#include <tlhelp32.h>
#include "my_memory.h"

using namespace std;

uintptr_t find_dynamic_addr(
    uintptr_t base_plus_static_offset,
    uintptr_t *offsets,
    int length,
    HANDLE hprocess)
{
    uintptr_t ptr{};
    ReadProcessMemory(hprocess, (LPCVOID)base_plus_static_offset, &ptr, sizeof(ptr), NULL);
    for (int counter{}; length > counter; counter++)
    {
        if (counter < length - 1)
        {
            ReadProcessMemory(hprocess, (LPCVOID)(ptr + offsets[counter]), &ptr, sizeof(ptr), NULL);
        }
        else
        {
            ptr += offsets[counter];
        }
    }
    return ptr;
}

uintptr_t GetModuleBase(HANDLE hProcess, const wstring &modName)
{
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        for (int i = 0; i < cbNeeded / sizeof(HMODULE); i++)
        {
            WCHAR modFileName[MAX_PATH];
            GetModuleFileNameExW(hProcess, hMods[i], modFileName, MAX_PATH);
            wstring fullPath(modFileName);
            if (fullPath.find(modName) != wstring::npos)
            {
                return (uintptr_t)hMods[i];
            }
        }
    }
    return 0;
}

DWORD GetPID(const wstring &processName)
{
    DWORD pid{};
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0);
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    if (Process32FirstW(snapshot, &pe32))
    {
        do
        {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0)
            {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);
    return pid;
}
