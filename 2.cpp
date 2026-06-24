#include <windows.h>
#include <algorithm>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <vector>

using namespace std;

const unsigned int player_obj_value{5558412};
struct Vector3{float x, y, z;};
struct Vector2{float x, y;};


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


uintptr_t find_ptr_to_dynamic_addr(
    uintptr_t struct_base_addr,
    uintptr_t *offsets,
    int length,
    HANDLE hprocess)
{
    uintptr_t ptr{};
    ReadProcessMemory(hprocess, (LPCVOID)struct_base_addr, &ptr, sizeof(ptr), NULL);
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


bool WorldToScreen(Vector3 worldPos, Vector2 &screenPos, float *vm, int screenW, int screenH){
    float clipX = worldPos.x * vm[0] + worldPos.y * vm[4] + worldPos.z * vm[8] + vm[12];
    float clipY = worldPos.x * vm[1] + worldPos.y * vm[5] + worldPos.z * vm[9] + vm[13];
    float clipW = worldPos.x * vm[3] + worldPos.y * vm[7] + worldPos.z * vm[11] + vm[15];
    if (clipW < 0.1f)
        return false;
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;
    screenPos.x = screenW / 2 + ndcX * screenW / 2;
    screenPos.y = screenH / 2 - ndcY * screenH / 2;
    return true;}


int players_count(
    uintptr_t base,
    uintptr_t *offsets,
    HANDLE hprocess)
{   
    base += 0x00191FCC;
    vector<uintptr_t> used_addr;
    uintptr_t copied_offsets[2];
    for(int counter{}; counter < 2; counter++){copied_offsets[counter] = offsets[counter];}
    int result_count{}, value{};
    while (true)
    {
        uintptr_t dynamic_addr = find_ptr_to_dynamic_addr(base, copied_offsets, 2, hprocess);
        if (
            ReadProcessMemory(hprocess, (LPCVOID)dynamic_addr, &value, sizeof(value), NULL)
            && value==player_obj_value
            && find(used_addr.begin(), used_addr.end(), dynamic_addr) == used_addr.end()
        ){
            used_addr.push_back(dynamic_addr);
            copied_offsets[0] += 0x4;
            result_count++;
        }
        else break;
    }
    return result_count;
}


int main()
{
    string processName = "ac_client.exe";
    DWORD pid = GetPID(wstring(processName.begin(), processName.end()));
    if (pid != 0)
    {
        cout << "process found. pid: " << pid << endl;
    }
    else
    {
        cout << "process not found." << endl;
    }
    HANDLE hprocess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        pid);
    if (!hprocess)
    {
        cout << "couldn't get access to the process." << endl;
        return 1;
    }
    uintptr_t base = GetModuleBase(hprocess, L"ac_client.exe");
    
    CloseHandle(hprocess);
    return 0;
}
