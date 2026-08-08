#define NOMINMAX
#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#endif
#include "memory.hpp"
#include <string>
#include <string_view>
#include <string.h>
#include <memory>
#include <fstream>
#include <mutex>
#include "vmmdll.h"
#include <iostream>
#include <vector>
#include <algorithm>

int Start(int argc, char** argv) {

    bool DLL = false;
    bool DLLInitialized = false;

    bool sys_present = false;

    const char* sys_string = ".sys";

    for(int i = 0; i < argc; i++) {
        if(strstr(*(argv + i), sys_string) != NULL){
            sys_present = true;
        }
    }

    if (sys_present){

        Initialize();

        driver_name = argv[1];

        printf("Target Driver: %s\n", argv[1]);
        if(InitializeDriver(argv[1])){
            if (!DumpDriver()) {
                printf("[!] Failed to dump driver\n");
            }
        } else {
            printf("[!] Failed to initialize driver\n");
        }

        return 0;

    }

    if(!sys_present){

    if (argc < 2)
    {
        printf("[!] Incorrect usage.\n[>] Usage: %s abc.exe", argv[0]);
        return -1;
    }

    printf("Target Executable: %s\n", argv[1]);

    if (argv[2]) {
        printf("Target DLL: %s\n", argv[2]);
        DLL = true;
    }
    else {
        printf("No DLL provided.\n");
    }

    if (!Initialize_with_exe(argv[1])) {
        printf("[!] Failed to initialize memory\n");

    }

    if (!DumpExe()) {
        printf("[!] Failed to dump exe\n");
    }

    if (DLL) {
        InitializeDLL(argv[1], argv[2]);
        DLLInitialized = true;

        if (DLLInitialized) {
            printf("DLL initialized");
        }

        if (!DumpDLL()) {
            printf("[!] Failed to dump dll\n");
        }
    }

    }

    return 0;

}

int main(int argc, char** argv)
{
    if (argc && argv) {
    Start(argc, argv);
}
}
