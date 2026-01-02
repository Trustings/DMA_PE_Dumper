#include "headers.hpp"

int Start(int argc, char** argv) {

    bool DLL = false;
    bool DLLInitialized = false;

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

    if (!Initialize(argv[1])) {
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


    return 0;

}

int main(int argc, char** argv)
{
    if (argc && argv) {
    Start(argc, argv);
}
}
