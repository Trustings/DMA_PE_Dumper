#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <string_view>
#include <memory>
#include <TlHelp32.h>
#include <fstream>
#include <mutex>
#include "vmmdll.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <thread>

struct Info
{
    uint32_t index;
    uint32_t process_id;
    uint64_t dtb;
    uint64_t kernelAddr;
    char name[256];
};

extern VMM_HANDLE hVMM;
extern std::string process_name;
extern std::string DLL_Name;
extern uint32_t process_id;
extern HANDLE process_handle;
extern ULONG64 process_base_address;
extern ULONG64 DLL_base_address;
extern DWORD process_size;
extern DWORD DLL_size;
extern HWND g_hOutput;

// Logging functions
extern void AddLog(const std::string& text);
void AppendOutput(const std::string& text);

// Color function declarations
void SetConsoleColor(int color);
void ResetConsoleColor();
void printf_red(const char* format, ...);
void printf_green(const char* format, ...);
void printf_yellow(const char* format, ...);
void printf_blue(const char* format, ...);
void printf_cyan(const char* format, ...);
void printf_magenta(const char* format, ...);

// Core dumper functions
bool Initialize(const std::string process_name);
bool InitializeDLL(const std::string process_name, const std::string DLL_Name);
VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo);
bool vmmdll_read(uint64_t address, void* buffer, size_t size);
template<class T> T read(uintptr_t address);
bool read_buffer(uintptr_t address, void* buffer, size_t size);
uint32_t get_process_id(const std::string process_name);
bool get_process_base_address(const std::string process_name, const uint32_t& process_id);
bool GetDLLModuleBase(const uint32_t& process_id, const std::string DLL_Name);
std::string get_path();
bool DumpExe();
bool DumpDLL();

// GUI function declarations
bool InitializeGUI(HWND hwnd);
void RenderFrame();
void ShutdownGUI();
void SetupImGuiStyle();
void RenderMainWindow();
void ResetLogWithInitialMessages();
void RunDumpThread(const std::string& exePath, const std::string& dllPath);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);