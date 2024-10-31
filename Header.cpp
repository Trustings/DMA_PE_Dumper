#include "Headers.hpp"

VMM_HANDLE hVMM = nullptr;
std::string process_name;
uint32_t process_id = 0;
HANDLE process_handle = nullptr;
ULONG64 process_base_address = 0;
DWORD process_size = 0;