#include "Headers.hpp"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("[!] Incorrect usage.\n[>] Usage: %s abc.exe", argv[0]);
        return -1;
    }

    if (!Initialize(argv[1]))
    {
        printf("[!] Failed to initialize memory\n");
        return -1;
    }

    if (!dump())
    {
        printf("[!] Failed to dump process\n");
        return -1;
    }

    return 0;
}
