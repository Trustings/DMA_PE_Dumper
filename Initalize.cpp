#include "headers.hpp"

bool Initialize(const std::string process_name)
{
    LPCSTR Parameters[] = { "", "-device", "fpga" };
    hVMM = VMMDLL_Initialize(3, Parameters);

    if (!hVMM) {
        printf("[!] Failed to initialize memory process file system in call to vmm.dll!VMMDLL_Initialize (Error: %d)\n", GetLastError());
        return false;
    }

    printf("[>] Init handle VMM success\n");


    process_id = get_process_id(process_name);

    printf("[+] Process id: %d\n", process_id);

    if (!process_id)
    {
        printf("[!] Failed to get process id of %s\n", process_name);
        return false;
    }

    if (!get_process_base_address(process_name, process_id))
    {
        printf("[+] Failed to get base address/size of process 0x%lX (Error: %d)\n", process_base_address, GetLastError());
        return false;
    }

    printf("[+] Base address: 0x%llX\n", process_base_address);
    printf("[+] Image size: 0x%llX\n", process_size);

    return true;
}