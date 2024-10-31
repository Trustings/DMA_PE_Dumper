#include "Headers.hpp"

bool dump()
{
    printf("[>] Dumping...\n");

    if (!process_id ||
        !process_base_address ||
        !process_size
        )
    {
        printf("[!] Memory is not initialized.\n");
        return false;
    }

    auto buffer = (BYTE*)malloc(process_size);

    if (!buffer)
    {
        printf("[!] Failed to allocate buffer (Error: %d)\n", GetLastError());
        return false;
    }

    printf("[+] Buffer allocated at 0x%p\n", buffer);

    for (ULONG iterator = 0x0; iterator < process_size; iterator += 0x1000) {
        size_t read_size = ((iterator + 0x1000) > process_size) ? (process_size - iterator) : 0x1000;
        if (!read_buffer(process_base_address + iterator, buffer + iterator, read_size)) {
            printf("[!] Failed to read buffer at 0x%lX (Error: %d)\n",
                process_base_address + iterator, GetLastError());
            free(buffer);
            return false;
        }
    }

    auto pdos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);

    if (!pdos_header->e_lfanew)
    {
        printf("[!] Failed to get dos header from buffer\n");
        free(buffer);
        return false;
    }

    printf("[+] Dos header readed: %p\n", pdos_header);

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("[!] Invalid dos header signature\n");
        free(buffer);
        return false;
    }

    auto pnt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(buffer + pdos_header->e_lfanew);

    if (!pnt_header)
    {
        printf("[!] Failed to read nt header from buffer\n");
        free(buffer);
        return false;
    }

    printf("[+] Nt header readed: 0x%p\n", pnt_header);

    if (pnt_header->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("[!] Invalid nt header signature from readed nt header\n");
        free(buffer);
        return false;
    }

    auto poptional_header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER>(&pnt_header->OptionalHeader);

    if (!poptional_header)
    {
        printf("[!] Failed to read optional header from buffer\n");
        free(buffer);
        return false;
    }

    printf("[+] Optional header readed: 0x%p\n", poptional_header);

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
            printf("[!] Failed to read buffer for section %s\n", psection_header->Name);
            free(buffer);
            return false;
        }
    }

    char FileName[MAX_PATH];
    sprintf_s(FileName, "%s%s_dump.exe", get_path().c_str(), process_name.c_str());

    std::ofstream Dump(FileName, std::ios::binary);
    Dump.write((char*)buffer, process_size);
    Dump.close();

    printf("[>] Dumped successfully to %s\n", FileName);
    free(buffer);

    return true;
}
