#include "Headers.hpp"

// External GUI functions
extern bool InitializeGUI(HWND hwnd);
extern void RenderFrame();
extern void ShutdownGUI();
extern LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Register window class
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"PE DMA Dumper Tool v3.0",
        nullptr
    };
    RegisterClassExW(&wc);

    // Create window
    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"PE DMA Dumper Tool v3.0",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1200, 800,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // Initialize GUI
    if (!InitializeGUI(hwnd)) {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Main message loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        RenderFrame();
    }

    // Cleanup
    ShutdownGUI();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}