#define IMGUI_DEFINE_MATH_OPERATORS
#include "Headers.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <thread>
#include <mutex>
#include <vector>

// DirectX 11 objects
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// GUI state variables
static char g_exe_path[256] = "";
static char g_dll_path[256] = "";
static bool g_dump_dll = false;
static bool g_is_dumping = false;
static bool g_log_errors = true;
static std::vector<std::string> g_log;
static std::mutex g_log_mutex;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Custom drawing functions
void DrawGradientBackground(ImVec2 pos, ImVec2 size, ImU32 col1, ImU32 col2) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        col1, col2, col2, col1);
}

bool CustomButton(const char* label, ImVec2 size, ImVec4 col, ImVec4 col_hov, ImVec4 col_act) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col_hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col_act);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void AnimatedToggle(const char* label, bool* v, ImVec2 size) {
    // Use a simple invisible button for the toggle area
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(label, size);
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();

    if (ImGui::IsItemClicked()) {
        *v = !*v;
    }

    // Colors for toggle
    ImU32 bg_color;
    if (*v) {
        bg_color = is_hovered ? IM_COL32(80, 140, 80, 255) : IM_COL32(70, 130, 70, 255);
    }
    else {
        bg_color = is_hovered ? IM_COL32(90, 90, 90, 255) : IM_COL32(80, 80, 80, 255);
    }

    // Draw background rectangle
    draw_list->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg_color, size.y * 0.5f);

    // Draw toggle circle
    float circle_radius = (size.y - 4.0f) * 0.5f;
    float circle_x = *v ? (p.x + size.x - circle_radius - 2.0f) : (p.x + circle_radius + 2.0f);
    ImVec2 circle_center = ImVec2(circle_x, p.y + size.y * 0.5f);

    draw_list->AddCircleFilled(circle_center, circle_radius, IM_COL32(255, 255, 255, 255));

    // Draw label if provided
    if (strlen(label) > 0) {
        ImGui::SameLine();
        ImGui::Text("%s", label);
    }
}

void AddLog(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_log_mutex);

    // Check if this is an error message and if error logging is disabled
    if (!g_log_errors && (text.find("[!]") != std::string::npos ||
        text.find("ERROR") != std::string::npos ||
        text.find("EXCEPTION") != std::string::npos ||
        text.find("Failed") != std::string::npos)) {
        return; // Skip logging this error message
    }

    g_log.push_back(text);
}

void ResetLogWithInitialMessages() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log.clear();
    g_log.push_back("DMA Dumper - Ready");
    g_log.push_back("Enter target executable name to begin");
}

void RunDumpThread(const std::string& exePath, const std::string& dllPath) {
    // Clear the initial messages when dump starts
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        g_log.clear();
    }

    AddLog("=== Starting Dump Operation ===");
    AddLog("Target: " + exePath);
    if (!dllPath.empty()) AddLog("DLL: " + dllPath);

    bool success = true;
    try {
        if (!Initialize(exePath)) {
            AddLog("ERROR: Failed to initialize memory");
            success = false;
            goto cleanup;
        }
        if (!DumpExe()) {
            AddLog("ERROR: Failed to dump executable");
            success = false;
            goto cleanup;
        }
        if (!dllPath.empty()) {
            if (InitializeDLL(exePath, dllPath)) {
                AddLog("DLL initialized successfully");
                if (!DumpDLL()) {
                    AddLog("ERROR: Failed to dump DLL");
                    success = false;
                }
            }
            else {
                AddLog("ERROR: Failed to initialize DLL");
                success = false;
            }
        }
    }
    catch (...) {
        AddLog("EXCEPTION: Unknown error occurred");
        success = false;
    }

cleanup:
    AddLog(success ? "=== DUMP COMPLETED SUCCESSFULLY ===" : "=== DUMP FAILED ===");
    g_is_dumping = false;
}

void SetupImGuiStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(12, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 25.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;

    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.95f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.67f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 0.63f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.80f, 0.60f, 1.00f);
}

void RenderMainWindow() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 16, io.DisplaySize.y - 16));
    ImGui::Begin("PE Dumper Tool", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse);

    // Input section with proper alignment
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Target Executable:");
    ImGui::SameLine();
    ImGui::SetCursorPosX(141);
    ImGui::PushItemWidth(250);
    ImGui::InputTextWithHint("##TargetExe", "e.g., UAGame.exe", g_exe_path, sizeof(g_exe_path));

    ImGui::SameLine();
    ImGui::SetCursorPosX(420);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("DLL Path (Optional):");
    ImGui::SameLine();
    ImGui::SetCursorPosX(560);
    ImGui::InputTextWithHint("##DLLPath", "e.g., kernel32.dll", g_dll_path, sizeof(g_dll_path));
    ImGui::PopItemWidth();

    ImGui::Separator();

    //// Control buttons
    //ImGui::AlignTextToFramePadding();
    //ImGui::Text("Operations:");
    //ImGui::SameLine();

    // Move buttons closer to the text by reducing the positioning offset
    float windowWidth = ImGui::GetWindowWidth();
    float buttonWidth = 120.0f;
    float toggleWidth = 80.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float totalWidth = (buttonWidth * 2) + toggleWidth + (spacing * 3);
    ImGui::SetCursorPosX(20);  

    // Start Dump button
    if (g_is_dumping) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.6f, 0.4f, 1.0f));
        ImGui::Button("Dumping...", ImVec2(buttonWidth, 35));
        ImGui::PopStyleColor(3);
    }
    else {
        if (CustomButton("Start Dump", ImVec2(buttonWidth, 35),
            ImVec4(0.2f, 0.6f, 0.2f, 1.0f),  // normal
            ImVec4(0.3f, 0.7f, 0.3f, 1.0f),  // hovered
            ImVec4(0.4f, 0.8f, 0.4f, 1.0f))) { // active
            if (strlen(g_exe_path) > 0) {
                g_is_dumping = true;
                std::string exePath = g_exe_path;
                std::string dllPath = strlen(g_dll_path) > 0 ? g_dll_path : "";
                std::thread dumpThread(RunDumpThread, exePath, dllPath);
                dumpThread.detach();
            }
        }
    }

    ImGui::SameLine();

    // Clear Log button
    if (CustomButton("Clear Log", ImVec2(buttonWidth, 35),
        ImVec4(0.6f, 0.3f, 0.2f, 1.0f),  // normal
        ImVec4(0.7f, 0.4f, 0.3f, 1.0f),  // hovered
        ImVec4(0.8f, 0.5f, 0.4f, 1.0f))) { // active
        ResetLogWithInitialMessages();
    }

    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.5f); 
    AnimatedToggle("Log Errors", &g_log_errors, ImVec2(55, 28));

    ImGui::Separator();

    //// Log section
    //ImGui::AlignTextToFramePadding();
    //ImGui::Text("Output Console:");

    ImGui::BeginChild("Output Log", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        for (const auto& line : g_log) {
            if (line.find("[!]") != std::string::npos || line.find("ERROR") != std::string::npos ||
                line.find("EXCEPTION") != std::string::npos || line.find("Failed") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            }
            else if (line.find("[+]") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            }
            else if (line.find("===") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
            }
            else if (line.find("[>]") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.6f, 1.0f, 1.0f));
            }
            else if (line.find("Target:") != std::string::npos || line.find("DLL:") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
            }

            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 50.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

bool InitializeGUI(HWND hwnd) {
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    SetupImGuiStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ResetLogWithInitialMessages();
    return true;
}

void RenderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderMainWindow();

    ImGui::Render();
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

void ShutdownGUI() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}