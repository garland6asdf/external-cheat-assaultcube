#include <windows.h>
#include <iostream>
#include <string>
#include <d3d11.h>
#include "esp.h"
#include "constants.h"
#include "my_memory.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;

ID3D11Device *g_device = nullptr;
ID3D11DeviceContext *g_context = nullptr;
IDXGISwapChain *g_swapChain = nullptr;
ID3D11RenderTargetView *g_renderTarget = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

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
    uintptr_t module_base = GetModuleBase(hprocess, L"ac_client.exe");
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MVP";

    if (!RegisterClassExW(&wc))
    {
        std::cout << "RegisterClassEx failed: "
                << GetLastError() << std::endl;
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"MVP", L"",
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    if (!hwnd)
    {
        cout << "CreateWindowEx failed: "
                << GetLastError() << endl;
        return 1;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_swapChain,
        &g_device,
        &fl,
        &g_context
    );

    if (FAILED(hr))
    {
        std::cout << "CreateDevice failed\n";
        return 1;
    }

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
    uintptr_t local_player_offsets[2]{0x0};
    uintptr_t player_address = find_dynamic_addr(module_base + Offsets::LOCAL_PLAYER, local_player_offsets, 2, hprocess);
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
        try
        {
            espbox(hprocess, module_base, player_address, draw); // ********** my func
        }
        catch (...)
        {
            continue;
        }
        ImGui::Render();

        float clear[4] = {0, 0, 0, 1};
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, clear);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapChain->Present(0, 0);
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
