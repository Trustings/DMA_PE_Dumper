#include "Headers.hpp"

bool DumpExe()
{
    printf_cyan("[>] Dumping executable: %s...\n", process_name.c_str());

    if (!process_id ||
        !process_base_address ||
        !process_size
        )
    {
        printf_red("[!] Memory is not initialized.\n");
        return false;
    }

    auto buffer = (BYTE*)malloc(process_size);

    if (!buffer)
    {
        printf_red("[!] Failed to allocate buffer (Error: %d)\n", GetLastError());
        return false;
    }

    printf_green("[+] Buffer allocated at 0x%p\n", buffer);

    for (ULONG iterator = 0x0; iterator < process_size; iterator += 0x1000) {
        size_t read_size = ((iterator + 0x1000) > process_size) ? (process_size - iterator) : 0x1000;
        if (!read_buffer(process_base_address + iterator, buffer + iterator, read_size)) {
            printf_red("[!] Failed to read buffer at 0x%lX (Error: %d)\n",
                process_base_address + iterator, GetLastError());
            free(buffer);
            return false;
        }
    }

    auto pdos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);

    if (!pdos_header->e_lfanew)
    {
        printf_red("[!] Failed to get dos header from buffer\n");
        free(buffer);
        return false;
    }

    printf_green("[+] Dos header readed: %p\n", pdos_header);

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf_red("[!] Invalid dos header signature\n");
        free(buffer);
        return false;
    }

    auto pnt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer + pdos_header->e_lfanew);

    if (!pnt_header)
    {
        printf_red("[!] Failed to read nt header from buffer\n");
        free(buffer);
        return false;
    }

    printf_green("[+] Nt header readed: 0x%p\n", pnt_header);

    if (pnt_header->Signature != IMAGE_NT_SIGNATURE)
    {
        printf_red("[!] Invalid nt header signature from readed nt header\n");
        free(buffer);
        return false;
    }

    auto poptional_header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(&pnt_header->OptionalHeader);

    if (!poptional_header)
    {
        printf_red("[!] Failed to read optional header from buffer\n");
        free(buffer);
        return false;
    }

    printf_green("[+] Optional header readed: 0x%p\n", poptional_header);

    int i = 0;
    unsigned int section_offset = poptional_header->SizeOfHeaders;

    for (
        PIMAGE_SECTION_HEADER psection_header = IMAGE_FIRST_SECTION(pnt_header);
        i < pnt_header->FileHeader.NumberOfSections;
        i++, psection_header++
        )
    {
        size_t section_size = std::max(psection_header->Misc.VirtualSize, psection_header->SizeOfRawData);

        memcpy(buffer + section_offset, psection_header, sizeof(IMAGE_SECTION_HEADER));
        section_offset += sizeof(IMAGE_SECTION_HEADER);

        if (!read_buffer(
            poptional_header->ImageBase + psection_header->VirtualAddress,
            buffer + psection_header->PointerToRawData,
            section_size
        ))
        {
            printf_red("[!] Failed to read buffer for section %s\n", psection_header->Name);
            free(buffer);
            return false;
        }
    }

    char FileName[MAX_PATH];
    std::string process_name_without_ext = process_name;
    size_t pos = process_name_without_ext.find_last_of(".");
    if (pos != std::string::npos) {
        process_name_without_ext = process_name_without_ext.substr(0, pos);
    }
    sprintf_s(FileName, "%s%s_Dump.exe", get_path().c_str(), process_name_without_ext.c_str());

    std::ofstream Dump(FileName, std::ios::binary);
    Dump.write((char*)buffer, process_size);
    Dump.close();

    printf_green("[>] Executable dumped successfully to %s\n", FileName);
    free(buffer);

    return true;
}

bool DumpDLL() {

    {
        printf_cyan("[>] Dumping DLL: %s...\n", DLL_Name.c_str());

        if (!process_id ||
            !DLL_base_address ||
            !DLL_size
            )
        {
            printf_red("[!] Memory is not initialized.\n");
            return false;
        }

        auto buffer = (BYTE*)malloc(DLL_size);

        if (!buffer)
        {
            printf_red("[!] Failed to allocate buffer (Error: %d)\n", GetLastError());
            return false;
        }

        printf_green("[+] Buffer allocated at 0x%p\n", buffer);

        for (ULONG iterator = 0x0; iterator < DLL_size; iterator += 0x1000) {
            size_t read_size = ((iterator + 0x1000) > DLL_size) ? (DLL_size - iterator) : 0x1000;
            if (!read_buffer(DLL_base_address + iterator, buffer + iterator, read_size)) {
                printf_red("[!] Failed to read buffer at 0x%lX (Error: %d)\n",
                    DLL_base_address + iterator, GetLastError());
                free(buffer);
                return false;
            }
        }

        auto pdos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);

        if (!pdos_header->e_lfanew)
        {
            printf_red("[!] Failed to get dos header from buffer\n");
            free(buffer);
            return false;
        }

        printf_green("[+] Dos header readed: %p\n", pdos_header);

        if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
        {
            printf_red("[!] Invalid dos header signature\n");
            free(buffer);
            return false;
        }

        auto pnt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer + pdos_header->e_lfanew);

        if (!pnt_header)
        {
            printf_red("[!] Failed to read nt header from buffer\n");
            free(buffer);
            return false;
        }

        printf_green("[+] Nt header readed: 0x%p\n", pnt_header);

        if (pnt_header->Signature != IMAGE_NT_SIGNATURE)
        {
            printf_red("[!] Invalid nt header signature from readed nt header\n");
            free(buffer);
            return false;
        }

        auto poptional_header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(&pnt_header->OptionalHeader);

        if (!poptional_header)
        {
            printf_red("[!] Failed to read optional header from buffer\n");
            free(buffer);
            return false;
        }

        printf_green("[+] Optional header readed: 0x%p\n", poptional_header);

        int i = 0;
        unsigned int section_offset = poptional_header->SizeOfHeaders;

        for (
            PIMAGE_SECTION_HEADER psection_header = IMAGE_FIRST_SECTION(pnt_header);
            i < pnt_header->FileHeader.NumberOfSections;
            i++, psection_header++
            )
        {
            size_t section_size = std::max(psection_header->Misc.VirtualSize, psection_header->SizeOfRawData);

            memcpy(buffer + section_offset, psection_header, sizeof(IMAGE_SECTION_HEADER));
            section_offset += sizeof(IMAGE_SECTION_HEADER);

            if (!read_buffer(
                poptional_header->ImageBase + psection_header->VirtualAddress,
                buffer + psection_header->PointerToRawData,
                section_size
            ))
            {
                printf_red("[!] Failed to read buffer for section %s\n", psection_header->Name);
                free(buffer);
                return false;
            }
        }

        char FileName[MAX_PATH];
        std::string dll_name_without_ext = DLL_Name;
        size_t pos = dll_name_without_ext.find_last_of(".");
        if (pos != std::string::npos) {
            dll_name_without_ext = dll_name_without_ext.substr(0, pos);
        }
        sprintf_s(FileName, "%s%s_Dump.dll", get_path().c_str(), dll_name_without_ext.c_str());

        std::ofstream Dump(FileName, std::ios::binary);
        Dump.write((char*)buffer, DLL_size);
        Dump.close();

        printf_green("[>] DLL dumped successfully to %s\n", FileName);
        free(buffer);

        return true;
    }

}