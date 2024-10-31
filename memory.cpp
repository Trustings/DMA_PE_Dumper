#include "Headers.hpp"

uint64_t cbSize = 0x80000;

VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo)
{
    if (strcmp(uszName, "dtb.txt") == 0)
        cbSize = cb;
}

bool vmmdll_read(uint64_t address, void* buffer, size_t size) {
    if (!VMMDLL_MemRead(hVMM, (DWORD)process_id, (ULONG64)address, (PBYTE)buffer, size)) {
        DWORD error_code = GetLastError();
        printf("[!] VMMDLL_MemRead failed at address 0x%llX with size %zu (Error: %d)\n", address, size, error_code);
        return false;
    }
    return true;
}

template<class T> T read(uintptr_t address)
{
    T buffer;
    vmmdll_read(address, &buffer, sizeof(T));
    return buffer;
}

bool read_buffer(uintptr_t address, void* buffer, size_t size)
{
    // Byfron read
    // Credits to https://www.unknowncheats.me/forum/3484102-post9127.html

    uint64_t read;
    MEMORY_BASIC_INFORMATION pbi;
    auto chunks_num = size / 0x1000;
    auto staraddr = (__int64)address;
    auto staraddrbuf = (__int64)buffer;

    for (size_t i = 0; i < chunks_num; i++)
    {
        auto remotepage = staraddr + 0x1000 * i;
        auto localpage = staraddrbuf + 0x1000 * i;
        VirtualQueryEx(process_handle, (void*)address, &pbi, sizeof(pbi));
        if (pbi.Protect != PAGE_NOACCESS)
        {
            vmmdll_read(remotepage, (void*)localpage, 0x1000);
        }
    }
    return 1;

    return vmmdll_read(address, buffer, size);
}

uint32_t get_process_id(const std::string process_name)
{
    DWORD dwPID;
    bool result = VMMDLL_PidGetFromName(hVMM, const_cast<char*>(process_name.c_str()), &dwPID);
    if (!result) {
        printf("[!] VMMDLL_PidGetFromName failed (Error: %d)\n", GetLastError());
        return 0; 
    }
    return dwPID;
}

bool get_process_base_address(const std::string process_name, const uint32_t& process_id)
{
    PVMMDLL_MAP_MODULEENTRY pModuleEntryExplorer;

    bool result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &pModuleEntryExplorer, NULL);

    if (result) {
        process_size = pModuleEntryExplorer->cbImageSize;
        process_base_address = pModuleEntryExplorer->vaBase;
        return true;
    }

    if (!VMMDLL_InitializePlugins(hVMM)) {
        printf("[-] Failed VMMDLL_InitializePlugins call\n");
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (true) {
        BYTE bytes[4] = { 0 };
        DWORD i = 0;
        auto nt = VMMDLL_VfsReadW(hVMM, (LPWSTR)L"\\misc\\procinfo\\progress_percent.txt", bytes, 3, &i, 0);
        if (nt == VMMDLL_STATUS_SUCCESS && atoi((LPSTR)bytes) == 100)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    VMMDLL_VFS_FILELIST2 VfsFileList;
    VfsFileList.dwVersion = VMMDLL_VFS_FILELIST_VERSION;
    VfsFileList.h = 0;
    VfsFileList.pfnAddDirectory = 0;
    VfsFileList.pfnAddFile = cbAddFile;  
    result = VMMDLL_VfsListU(hVMM, (LPSTR)"\\misc\\procinfo\\", &VfsFileList);
    if (!result) {
        printf("VMMDLL_VfsListU failed: %d\n", GetLastError());
        return false;
    }

    const size_t buffer_size = cbSize;
    BYTE* bytes = new BYTE[buffer_size];
    DWORD bytesRead = 0;
    auto nt = VMMDLL_VfsReadW(hVMM, (LPWSTR)L"\\misc\\procinfo\\dtb.txt", bytes, buffer_size - 1, &bytesRead, 0);
    if (nt != VMMDLL_STATUS_SUCCESS) {
        printf("VMMDLL_VfsReadW failed with code %d\n", nt);
        delete[] bytes;
        return false;
    }

    std::vector<uint64_t> possibleDTBs;
    char* pLineStart = reinterpret_cast<char*>(bytes);
    for (size_t i = 0; i < 1000; ++i) { 
        char* pLineEnd = strchr(pLineStart, '\n');
        if (pLineEnd == nullptr)
            break;

        *pLineEnd = '\0';  

        Info info = {};
        char format[] = "%X %X %llX %llX %s";
        int numFields = sscanf_s(pLineStart, format, &info.index, &info.process_id, &info.dtb, &info.kernelAddr, info.name, (unsigned)_countof(info.name));

        if (numFields == 5) {
            if (info.process_id == 0 || process_name.find(info.name) != std::string::npos) {
                possibleDTBs.push_back(info.dtb);
            }
        }

        pLineStart = pLineEnd + 1;
    }

    delete[] bytes;

    printf("Total DTBs to try: %zu\n", possibleDTBs.size());
    for (size_t i = 0; i < possibleDTBs.size(); i++) {
        auto dtb = possibleDTBs[i];
        VMMDLL_ConfigSet(hVMM, VMMDLL_OPT_PROCESS_DTB | process_id, dtb);

        result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &pModuleEntryExplorer, NULL);

        if (result) {
            process_base_address = pModuleEntryExplorer->vaBase;
            process_size = pModuleEntryExplorer->cbImageSize;
            printf("[+] Successfully patched DTB at index %zu with DTB 0x%llX.\n", i, dtb);
            return true;
        }
        else {
            printf("[!] Failed DTB patch attempt %zu with DTB 0x%llX (Error: %d)\n", i, dtb, GetLastError());
        }
    }

    printf("[-] Unable to patch DTB for access.\n");
    return false;
}