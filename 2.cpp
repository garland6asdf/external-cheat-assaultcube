#include <windows.h>
#include <algorithm>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <unordered_set>
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
    constexpr uintptr_t VIEW_MATRIX = 0x0057DFD0;
    constexpr uintptr_t ENTITY_LIST = 0x00191FCC;
    constexpr uintptr_t ENTITY_OFFSET = 0xE8;
    constexpr uintptr_t TEAM_ID = 0x30C;
    constexpr uintptr_t HEALTH = 0x4;
    constexpr uintptr_t X_COORD = 0x4;
}
namespace TimeDelay
{
    constexpr int RESCAN_PLAYERS_ADDR_INTERVAL{30};
}
const unsigned int PLAYER_OBJ_VALUE{5558412};
struct Vector3
{
    float x, y, z;
};
struct Vector2
{
    float x, y;
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

bool WorldToScreen(Vector3 worldPos, Vector2 &screenPos, float *vm, int screenW, int screenH)
{
    float clipX = worldPos.x * vm[0] + worldPos.y * vm[4] + worldPos.z * vm[8] + vm[12];
    float clipY = worldPos.x * vm[1] + worldPos.y * vm[5] + worldPos.z * vm[9] + vm[13];
    float clipW = worldPos.x * vm[3] + worldPos.y * vm[7] + worldPos.z * vm[11] + vm[15];
    if (clipW < 0.1f)
        return false;
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;
    screenPos.x = screenW / 2 + ndcX * screenW / 2;
    screenPos.y = screenH / 2 - ndcY * screenH / 2;
    return true;
}

void espbox(
    HANDLE hprocess,
    uintptr_t base,
    ImDrawList *draw)
{
    base += Offsets::ENTITY_LIST;
    static int frame_counter{};
    static std::vector<uintptr_t> cached_entities;
    float view_matrix[16];
    ReadProcessMemory(hprocess, (LPCVOID)Offsets::VIEW_MATRIX, &view_matrix, sizeof(view_matrix), NULL);

    constexpr size_t MAX_NECESSARY_OFFSET = Offsets::TEAM_ID + sizeof(int32_t);
    uint8_t buffer[MAX_NECESSARY_OFFSET];

    if (frame_counter % TimeDelay::RESCAN_PLAYERS_ADDR_INTERVAL == 0)
    {
        cached_entities.clear();
        uintptr_t offsets_entity_obj[2] = {0x0, Offsets::ENTITY_OFFSET};
        unordered_set<uintptr_t> used_addr;

        while (true)
        {
            uintptr_t dynamic_addr = find_ptr_to_dynamic_addr(base, offsets_entity_obj, 2, hprocess);
            uintptr_t entity_base = dynamic_addr - Offsets::ENTITY_OFFSET;

            if (
                ReadProcessMemory(hprocess, (LPCVOID)entity_base, &buffer, MAX_NECESSARY_OFFSET, NULL) &&
                *reinterpret_cast<int32_t *>(buffer + Offsets::ENTITY_OFFSET) == PLAYER_OBJ_VALUE &&
                !used_addr.count(dynamic_addr))
            {
                used_addr.insert(dynamic_addr);
                cached_entities.push_back(entity_base);
                offsets_entity_obj[0] += sizeof(int32_t);
            }
            else
                break;
        }
    }
    frame_counter++;
    for (uintptr_t entity_base : cached_entities)
    {
        if (!ReadProcessMemory(hprocess, (LPCVOID)entity_base, &buffer, MAX_NECESSARY_OFFSET, NULL))
            continue;

        if (*reinterpret_cast<int32_t *>(buffer + 0xe8) != PLAYER_OBJ_VALUE)
            continue;

        Vector3 feet_pos = *reinterpret_cast<Vector3 *>(buffer + Offsets::X_COORD);
        feet_pos.z -= 4.5f;
        int32_t team_id = *reinterpret_cast<int32_t *>(buffer + Offsets::TEAM_ID);
        int32_t hp = *reinterpret_cast<int32_t *>(buffer + Offsets::ENTITY_OFFSET + Offsets::HEALTH);
        Vector3 head_pos = {feet_pos.x, feet_pos.y, feet_pos.z + 5.2f};
        Vector2 feet_screen, head_screen;

        if ((hp >= 1 && hp <= 100) &&
            team_id == 1 &&
            WorldToScreen(feet_pos, feet_screen, view_matrix, 1920, 1080) &&
            WorldToScreen(head_pos, head_screen, view_matrix, 1920, 1080))
        {
            float height = feet_screen.y - head_screen.y;
            float width = height * 0.35f;

            draw->AddRect(
                ImVec2(feet_screen.x - width / 2, head_screen.y),
                ImVec2(feet_screen.x + width / 2, feet_screen.y),
                IM_COL32(255, 0, 0, 255));
        }
    }
}


void aimbot(){}


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
