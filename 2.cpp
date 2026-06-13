#include <windows.h>
#include <algorithm>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <psapi.h>

using namespace std;


std::wstring toLower(std::wstring str) {
    transform(str.begin(), str.end(), str.begin(), ::towlower);
    return str;
}


uintptr_t GetModuleBase(HANDLE hProcess, const wstring& modName) {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (int i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
            WCHAR modFileName[MAX_PATH];
            GetModuleFileNameExW(hProcess, hMods[i], modFileName, MAX_PATH);
            wstring fullPath(modFileName);
            if (fullPath.find(modName) != wstring::npos) {
                return (uintptr_t)hMods[i];
            }
        }
    }
    return 0;
}


DWORD GetPID(const wstring& processName){
    DWORD pid{};
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        }
        while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);
    return pid;
}


uintptr_t find_ptr_to_dynamic_addr(
    uintptr_t struct_base_addr,
    uintptr_t* offsets,
    int length,
    HANDLE hprocess
){
    uintptr_t ptr{};
    ReadProcessMemory(hprocess, (LPCVOID)struct_base_addr, &ptr, sizeof(ptr), NULL);
    for (int counter{};length>counter; counter++){
        if (counter < length - 1){
            ReadProcessMemory(hprocess, (LPCVOID)(ptr+offsets[counter]), &ptr, sizeof(ptr), NULL);
        }
        else {
            ptr += offsets[counter];
        }
    }
    return ptr;
}


int wmain(){
    wstring processName = L"ac_client.exe";
    DWORD pid = GetPID(processName);
    if (pid!=0){
        wcout << L"process found. pid: " << pid << endl;
    }
    else {
        wcout << L"process not found." << endl;
    }
    HANDLE hprocess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        pid
    );
    if (!hprocess){ 
        wcout << L"couldn't get access to the process." << endl;
        return 1;
    }

    int rrr{1337};
    uintptr_t base = GetModuleBase(hprocess, L"ac_client.exe");
    uintptr_t addr = base + 0x000DEE64;
    uintptr_t rifle_patrons_offsets[] = {0x3D4, 0x36C, 0x14, 0x0};
    uintptr_t health_offsets[] = {0x3D4, 0x36C, 0x8, 0xEC};
    uintptr_t armor_offsets[] = {0x3D4, 0x36C, 0x8, 0xF0};
    uintptr_t grenade_offsets[] = {0x3D4, 0x36C, 0x14, 0xC};

    uintptr_t rifle_patrons_ptr = find_ptr_to_dynamic_addr(addr, rifle_patrons_offsets, 4, hprocess);
    WriteProcessMemory(hprocess, (LPVOID)rifle_patrons_ptr, &rrr, sizeof(rrr), NULL);

    uintptr_t health_ptr = find_ptr_to_dynamic_addr(addr, health_offsets, 4, hprocess);
    WriteProcessMemory(hprocess, (LPVOID)health_ptr, &rrr, sizeof(rrr), NULL);

    uintptr_t armor_ptr = find_ptr_to_dynamic_addr(addr, armor_offsets, 4, hprocess);
    WriteProcessMemory(hprocess, (LPVOID)armor_ptr, &rrr, sizeof(rrr), NULL);
    
    uintptr_t grenade_ptr = find_ptr_to_dynamic_addr(addr, grenade_offsets, 4, hprocess);
    WriteProcessMemory(hprocess, (LPVOID)grenade_ptr, &rrr, sizeof(rrr), NULL);
    
    CloseHandle(hprocess);
    return 0;
}
