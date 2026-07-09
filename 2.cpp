#include <windows.h>
#include <algorithm>
#include <array>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <optional>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;

namespace Offsets
{   
    constexpr uintptr_t LOCAL_PLAYER = 0x17F110;
    constexpr uintptr_t ENTITY_LIST = 0x191FCC;
    constexpr uintptr_t TEAM_ID = 0x30C;
    constexpr uintptr_t HEALTH = 0xEC;
    constexpr uintptr_t X_COORD = 0x4;
    constexpr uintptr_t PLAYERS_COUNT = 0x18AC0C;
    constexpr uintptr_t VIEW_MATRIX = 0x57DFD0;
}

namespace TimeDelay
{
    constexpr int RESCAN_PLAYERS_ADDR_INTERVAL{30};
}

namespace Constants
{
    constexpr unsigned int PLAYER_OBJ_VALUE{5558396};
    constexpr uintptr_t MAX_NECESSARY_OFFSET = Offsets::TEAM_ID + sizeof(int32_t);
    constexpr uint32_t UNDEFINED{666666};
}

struct Vector3
{
    float x, y, z;
};


struct Vector2
{
    float x, y;
};


class PlayerData
{
    uintptr_t address;
    Vector3 feet_pos, head_pos;
    uint32_t hp, team_id;
    

    array<uint8_t, Constants::MAX_NECESSARY_OFFSET> get_buffer(HANDLE hprocess)
    {
        array<uint8_t, Constants::MAX_NECESSARY_OFFSET> buffer{};
        if (ReadProcessMemory(hprocess, (LPCVOID)address, buffer.data(), Constants::MAX_NECESSARY_OFFSET, NULL)){
            return buffer;
        }
        else{
            throw runtime_error("couldn't read the process memory.");
        }
        
    }

    static uint32_t get_players_quantity(HANDLE hprocess)
    {
        uint32_t players_quantity{};
        if (ReadProcessMemory(hprocess, (LPCVOID)Offsets::PLAYERS_COUNT, &players_quantity, sizeof(players_quantity), NULL)){
            return players_quantity;
        }
        else{
            throw runtime_error("couldn't read the process memory.");
        }
    }

public:

    static vector<uintptr_t> get_players_addresses(HANDLE hprocess, uintptr_t module_base)
    {
        module_base += Offsets::ENTITY_LIST;
        uint32_t players_quantity = get_players_quantity(hprocess);
        vector<uintptr_t> result;
        uintptr_t offsets[1] = {0x0};
        for (uint8_t counter{}; counter < players_quantity; counter++)
        {
            uintptr_t found_address = find_dynamic_addr(module_base, offsets, 1, hprocess);
            result.push_back(found_address);
            offsets[0] += 0x4; // читаем некст плеера
        }
        return result;
    }

    void init(HANDLE hprocess, uintptr_t entered_address)
    {
        address = entered_address;
        auto buffer = get_buffer(hprocess);
        if (*reinterpret_cast<int32_t *>(buffer.data()) != Constants::PLAYER_OBJ_VALUE)
        {
            throw invalid_argument("address is not valid.");
        }
        feet_pos = *reinterpret_cast<Vector3 *>(buffer.data() + Offsets::X_COORD);
        head_pos = {feet_pos.x, feet_pos.y, feet_pos.z + 5.2f};
        hp = *reinterpret_cast<uint32_t *>(buffer.data() + Offsets::HEALTH);
        team_id = *reinterpret_cast<uint32_t *>(buffer.data() + Offsets::TEAM_ID);
    }

    bool is_enemy(HANDLE hprocess, uintptr_t local_player_address)
    {   if (team_id != Constants::UNDEFINED){
            uint32_t local_player_team_id;
            if (ReadProcessMemory(hprocess, (LPCVOID)(local_player_address + Offsets::TEAM_ID), &local_player_team_id, sizeof(local_player_team_id), NULL)){
                return local_player_team_id != team_id;
            }
            else{
                throw runtime_error("couldn't read the process memory.");
            }
        }
        else{
            throw logic_error("initialize the player's parameters first.");
        }
    }

    bool is_alive()
    {
        return (hp > 0 && hp <= 100);
    }
};


ID3D11Device *g_device = nullptr;
ID3D11DeviceContext *g_context = nullptr;
IDXGISwapChain *g_swapChain = nullptr;
ID3D11RenderTargetView *g_renderTarget = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

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

uintptr_t find_dynamic_addr(
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

uintptr_t find_ptr_to_dynamic_addr(
    uintptr_t module_base,
    uintptr_t *offsets,
    int length,
    HANDLE hprocess)
{
    module_base += Offsets::ENTITY_LIST;
    uintptr_t ptr{};
    ReadProcessMemory(hprocess, (LPCVOID)module_base, &ptr, sizeof(ptr), NULL);
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

void CalcAngles(const Vector3 &local_pos, const Vector3 &target_pos, float &pitch, float &yaw) // ne ya pisa'l(*)
{
    Vector3 delta = {
        target_pos.x - local_pos.x,
        target_pos.y - local_pos.y,
        target_pos.z - local_pos.z};

    float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

    yaw = atan2(delta.y, delta.x) * 180.0f / M_PI;

    pitch = -atan2(delta.z, hyp) * 180.0f / M_PI;

    if (yaw < 0)
        yaw += 360.0f;
    if (yaw > 360)
        yaw -= 360.0f;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

void espbox(){}

void aimbot(HANDLE hprocess, uintptr_t base){}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
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

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MVP";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"MVP", L"",
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &g_swapChain, &g_device, &fl, &g_context);

    ID3D11Texture2D *buf = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&buf));
    g_device->CreateRenderTargetView(buf, nullptr, &g_renderTarget);
    buf->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    ShowWindow(hwnd, SW_SHOW);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0,
                 GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN),
                 SWP_NOMOVE | SWP_NOSIZE);

    bool running = true;
    while (running)
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImDrawList *draw = ImGui::GetForegroundDrawList();

        espbox(hprocess, base, draw); // ********** my func

        ImGui::Render();

        float clear[4] = {0, 0, 0, 1};
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, clear);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    g_renderTarget->Release();
    g_swapChain->Release();
    g_context->Release();
    g_device->Release();

    DestroyWindow(hwnd);
    CloseHandle(hprocess);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
