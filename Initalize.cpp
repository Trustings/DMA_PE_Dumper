#include "headers.hpp"

bool Initialize(const std::string process_name_param)
{
    process_name = process_name_param;

    LPCSTR Parameters[] = { "", "-device", "fpga" };
    hVMM = VMMDLL_Initialize(3, Parameters);

    if (!hVMM) {
        printf_red("[!] Failed to initialize memory process file system in call to vmm.dll!VMMDLL_Initialize (Error: %d)\n", GetLastError());
        return false;
    }

    printf_green("[>] Init handle VMM success\n");

    process_id = get_process_id(process_name);

    printf_green("[+] Process id: %d\n", process_id);

    if (!process_id)
    {
        printf_red("[!] Failed to get process id of %s\n", process_name.c_str());
        return false;
    }

    if (!get_process_base_address(process_name, process_id))
    {
        printf_red("[!] Failed to get base address/size of process 0x%lX (Error: %d)\n", process_base_address, GetLastError());
        return false;
    }

    printf_green("[+] Base address: 0x%llX\n", process_base_address);
    printf_green("[+] Image size: 0x%llX\n", process_size);

    return true;
}

bool InitializeDLL(const std::string process_name_param, const std::string DLL_Name_param)
{
    DLL_Name = DLL_Name_param;

    printf_green("[+] Process id: %d\n", process_id);

    if (!process_id)
    {
        printf_red("[!] Failed to get process id of %s\n", process_name_param.c_str());
        return false;
    }

    if (!GetDLLModuleBase(process_id, DLL_Name))
    {
        printf_red("[!] Failed to get base address/size of DLL 0x%lX (Error: %d)\n", DLL_base_address, GetLastError());
        return false;
    }

    printf_green("[+] DLL Base address: 0x%llX\n", DLL_base_address);
    printf_green("[+] DLL Image size: 0x%llX\n", DLL_size);

    return true;
}