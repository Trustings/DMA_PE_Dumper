#pragma once
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
extern uint32_t process_id;
extern HANDLE process_handle;
extern ULONG64 process_base_address;
extern DWORD process_size;

int main(int argc, char** argv);

bool Initialize(const std::string process_name);

VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo);

bool vmmdll_read(uint64_t address, void* buffer, size_t size);

template<class T> T read(uintptr_t address);

bool read_buffer(uintptr_t address, void* buffer, size_t size);

uint32_t get_process_id(const std::string process_name);

bool get_process_base_address(const std::string process_name, const uint32_t& process_id);

std::string get_path();

bool dump();

