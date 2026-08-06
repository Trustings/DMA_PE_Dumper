#include "memory.hpp"

uint32_t pid = 0;

uint64_t gafAsyncKeyStateExport = 0;
uint8_t state_bitmap[64]{ };
uint8_t previous_state_bitmap[256 / 8]{ };
uint64_t win32kbase = 0;

int win_logon_pid = 0;

std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();


VMM_HANDLE hVMM = nullptr;
std::string process_name;
std::string DLL_Name;
uint32_t process_id = 0;
HANDLE process_handle = nullptr;
ULONG64 process_base_address = 0;
ULONG64 DLL_base_address = 0;
DWORD process_size = 0;
DWORD DLL_size = 0;

uint64_t cbSize = 0x80000;

#ifdef LINUX

DWORD GetLastError() {
    return errno;
}

std::vector<pid_t> getPidsByName(const std::string& processName) {
    std::vector<pid_t> pids;

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (entry.is_directory()) {
            try {
                pid_t pid = std::stoi(entry.path().filename());
                std::ifstream cmdlineFile(entry.path() / "cmdline");
                std::string cmdline;
                if (std::getline(cmdlineFile, cmdline)) {
                    if (cmdline.find(processName) != std::string::npos) {
                        pids.push_back(pid);
                    }
                }
            } catch (...) {
                continue;
            }
        }
    }
    return pids;
}

bool Initialize(const std::string process_name_)
{

	process_name = process_name_;

    auto pids = getPidsByName("qemu-system-x86");
        pid_t pid = pids[0];

       // if (!pids.empty()){

        //-mount /mnt/memproc -v

           // continue;

       // std::cout << "PID: " << pid << std::endl;
        //}
        std::string url = "qemu://hugepage-pid=" + std::to_string(pid) + ",qmp=/tmp/qmp-win10.sock";

           // std::cout << url << std::endl;

        LPCSTR Parameters[] = {"", "-device", url.c_str(), "-mount", "/mnt/memproc", "-v", NULL};

        hVMM = VMMDLL_Initialize(6, Parameters);
        DWORD error_code;

        if (!hVMM) {
            printf("[!] Failed to initialize memory process file system in call to vmm.dll!VMMDLL_Initialize (Error: %d)\n", error_code);
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
            printf("[+] Failed to get base address/size of process 0x%lX (Error: %d)\n", process_base_address, error_code);
            return false;
        }

        printf("[+] Base address: 0x%llX\n", process_base_address);
        printf("[+] Image size: 0x%llX\n", process_size);

        return true;

}

bool InitializeDLL(const std::string process_name, const std::string DLL_Name)
{

    printf("[+] Process id: %d\n", process_id);

    if (!process_id)
    {
        printf("[!] Failed to get process id of %s\n", process_name);

    }

    if (!GetDLLModuleBase(process_id, DLL_Name))
    {
        printf("[+] Failed to get base address/size of process 0x%lX (Error: %d)\n", DLL_base_address, GetLastError());

    }

    printf("[+] Base address: 0x%llX\n", DLL_base_address);
    printf("[+] Image size: 0x%llX\n", DLL_size);

    return true;

}

#endif

#ifdef _WIN32
bool Initialize(const std::string process_name_)
{

	process_name = process_name_;
	
    LPCSTR Parameters[] = { "", "-device", "fpga" };

    hVMM = VMMDLL_Initialize(3, Parameters);
    DWORD error_code;

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

bool FixCr3_1()
{
    PVMMDLL_MAP_MODULEENTRY module_entry;
    bool result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, (LPSTR)process_name.c_str(), &module_entry, NULL);
    if (result)
        return true; //Doesn't need to be patched lol

    if (!VMMDLL_InitializePlugins(hVMM))
    {
        ERROR("[-] Failed VMMDLL_InitializePlugins call");
        return false;
    }

    //have to sleep a little or we try reading the file before the plugin initializes fully
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (true)
    {
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
    VfsFileList.pfnAddFile = cbAddFile; //dumb af callback who made this system

    result = VMMDLL_VfsListU(hVMM, (LPSTR)"\\misc\\procinfo\\", &VfsFileList);
    if (!result)
        return false;

    //read the data from the txt and parse it
    const size_t buffer_size = cbSize;
    std::unique_ptr<BYTE[]> bytes(new BYTE[buffer_size]);
    DWORD j = 0;
    auto nt = VMMDLL_VfsReadW(hVMM, (LPWSTR)L"\\misc\\procinfo\\dtb.txt", bytes.get(), buffer_size - 1, &j, 0);
    if (nt != VMMDLL_STATUS_SUCCESS)
        return false;

    std::vector<uint64_t> possible_dtbs;
    std::string lines(reinterpret_cast<char*>(bytes.get()));
    std::istringstream iss(lines);
    std::string line;

    while (std::getline(iss, line))
    {
        Info info = { };

        std::istringstream info_ss(line);
        if (info_ss >> std::hex >> info.index >> std::dec >> info.process_id >> std::hex >> info.dtb >> info.kernelAddr >> info.name)
        {
            if (info.process_id == 0) //parts that lack a name or have a NULL pid are suspects
                possible_dtbs.push_back(info.dtb);
            if (process_name.find(info.name) != std::string::npos)
                possible_dtbs.push_back(info.dtb);
        }
    }

    //loop over possible dtbs and set the config to use it til we find the correct one
    for (size_t i = 0; i < possible_dtbs.size(); i++)
    {
        auto dtb = possible_dtbs[i];
        VMMDLL_ConfigSet(hVMM, VMMDLL_OPT_PROCESS_DTB | process_id, dtb);
        result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, (LPSTR)process_name.c_str(), &module_entry, NULL);
        if (result)
        {
            printf("Patched DTB");
            return true;
        }
    }

    ERROR("[-] Failed to patch module");
    return false;
}

bool InitializeDLL(const std::string process_name, const std::string DLL_Name)
{

    printf("[+] Process id: %d\n", process_id);

    if (!process_id)
    {
        printf("[!] Failed to get process id of %s\n", process_name);

    }

    if (!GetDLLModuleBase(process_id, DLL_Name))
    {
        printf("[+] Failed to get base address/size of process 0x%lX (Error: %d)\n", DLL_base_address, GetLastError());

    }

    printf("[+] Base address: 0x%llX\n", DLL_base_address);
    printf("[+] Image size: 0x%llX\n", DLL_size);

    return true;

}

#endif

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

#ifdef _WIN32
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
#endif

VMMDLL_SCATTER_HANDLE CreateScatterHandle()
{
    return VMMDLL_Scatter_Initialize(hVMM, process_id, VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
}

void CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle)
{
    VMMDLL_Scatter_CloseHandle(handle);
}

void AddScatterRead(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
    VMMDLL_Scatter_PrepareEx(handle, address, size, static_cast<PBYTE>(buffer), NULL);
}

void ExecuteScatterRead(VMMDLL_SCATTER_HANDLE handle)
{
    VMMDLL_Scatter_ExecuteRead(handle);
    VMMDLL_Scatter_Clear(handle, process_id, VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
}

bool vmmdll_write(uint64_t address, void* buffer, size_t size) {
    if (!VMMDLL_MemWrite(hVMM, (DWORD)process_id, (ULONG64)address, (PBYTE)buffer, size)) {
        DWORD error_code = GetLastError();
        printf("[!] VMMDLL_MemWrite failed at address 0x%llX with size %zu (Error: %d)\n", address, size, error_code);
        return false;
    }

    return true;
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

#ifdef LINUX
static void force_unmount(const std::string& mountPoint = "/mnt/memproc") {
    // Try umount first
    int result = umount(mountPoint.c_str());
    if (result == 0) {
        std::cout << "Successfully unmounted " << mountPoint << std::endl;
        return;
    }

    // If umount fails, try lazy umount
    result = umount2(mountPoint.c_str(), MNT_DETACH);
    if (result == 0) {
        std::cout << "Lazy unmounted " << mountPoint << std::endl;
        return;
    }

    // If still failing, try force umount with fusermount
    std::string cmd = "fusermount -uz " + mountPoint + " 2>/dev/null";
    if (system(cmd.c_str()) == 0) {
        std::cout << "Force unmounted " << mountPoint << " with fusermount" << std::endl;
        return;
    }

    // Last resort: try to kill any remaining fuse processes
    std::string killCmd = "pkill -f 'memprocfs.*" + mountPoint + "' 2>/dev/null";
    system(killCmd.c_str());

    std::cout << "Attempted to clean up " << mountPoint << std::endl;
}

// Kill any existing memprocfs processes
static void kill_existing_memprocfs() {
    // Kill any existing memprocfs processes
    std::string killCmd = "pkill -f 'memprocfs.*-mount /mnt/memproc' 2>/dev/null";
    system(killCmd.c_str());

    // Force unmount
    force_unmount("/mnt/memproc");

    // Wait a moment for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

static pid_t g_memprocPid = -1;
// Launch memprocfs with suppressed output
static bool init_memprocfs(const std::string& qmpSocket = "/tmp/qmp-win10-1.sock") {
    // First, clean up any old mount and kill existing instances
    kill_existing_memprocfs();

    // Get QEMU process IDs
    auto pids = getPidsByName("qemu-system-x86");

    if (pids.empty()) {
        std::cerr << "No QEMU process found" << std::endl;
        return false;
    }

    pid_t qemuPid = pids[0];
    std::cout << "Found QEMU PID: " << qemuPid << std::endl;

    // Build the URL
    std::string url = "qemu://hugepage-pid=" + std::to_string(qemuPid) + ",qmp=" + qmpSocket;
    std::cout << "Launching: ./memprocfs -device " << url << " -mount /mnt/memproc -v" << std::endl;

    // Fork and execute
    pid_t childPid = fork();

    if (childPid == -1) {
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    }

    if (childPid == 0) {
        // Child process - execute memprocfs with output suppressed
        // Redirect stdout and stderr to /dev/null
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        execlp("/home/hermes/project/memprocfs/build/Desktop-Debug/memprocfs",
               "memprocfs",
               "-device",
               url.c_str(),
               "-mount",
               "/mnt/memproc",
               "-v",
               NULL);

        // If we get here, execlp failed
        std::cerr << "Failed to execute memprocfs: " << strerror(errno) << std::endl;
        exit(1);
    }

    // Store the PID globally
    g_memprocPid = childPid;

    // Wait 3 seconds for it to initialize
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Check if process is still running
    if (kill(childPid, 0) == 0) {
        std::cout << "memprocfs launched successfully with PID: " << childPid << std::endl;
        return true;
    } else {
        std::cerr << "memprocfs terminated during startup" << std::endl;
        g_memprocPid = -1;
        return false;
    }
}

// Terminate memprocfs with proper cleanup
static void terminate_memprocfs() {
    if (g_memprocPid <= 0) {
        std::cout << "memprocfs not running or already terminated" << std::endl;
        return;
    }

    std::cout << "Terminating memprocfs (PID: " << g_memprocPid << ")" << std::endl;

    // First, try to unmount cleanly
    std::cout << "Unmounting /mnt/memproc..." << std::endl;
    int umountResult = umount("/mnt/memproc");
    if (umountResult != 0) {
        // Try lazy unmount
        umount2("/mnt/memproc", MNT_DETACH);
        std::cout << "Used lazy unmount" << std::endl;
    } else {
        std::cout << "Unmounted successfully" << std::endl;
    }

    // Try graceful termination
    if (kill(g_memprocPid, SIGTERM) == 0) {
        // Wait up to 3 seconds for graceful termination
        int status;
        for (int i = 0; i < 15; i++) {
            pid_t result = waitpid(g_memprocPid, &status, WNOHANG);
            if (result == g_memprocPid) {
                std::cout << "memprocfs terminated gracefully" << std::endl;
                g_memprocPid = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // If still running, force kill
        std::cout << "memprocfs didn't respond to SIGTERM, forcing kill..." << std::endl;
        if (kill(g_memprocPid, SIGKILL) == 0) {
            waitpid(g_memprocPid, &status, 0);
            std::cout << "memprocfs force killed" << std::endl;
        }
    } else {
        // Process doesn't exist anymore
        std::cerr << "memprocfs already terminated" << std::endl;
    }

    // Final cleanup - force unmount if still mounted
    force_unmount("/mnt/memproc");

    g_memprocPid = -1;
}


bool FixCr3_1()
{
    init_memprocfs();

    // First try direct lookup
    PVMMDLL_MAP_MODULEENTRY module_entry;
    if (VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &module_entry, NULL))
    {
        printf("[+] DTB already correct\n");
        VMMDLL_MemFree(module_entry);

        terminate_memprocfs();
        return true;
    }

    if (!VMMDLL_InitializePlugins(hVMM))
    {
        printf("[-] Failed VMMDLL_InitializePlugins call\n");

        terminate_memprocfs();
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Wait for progress
    for (int wait = 0; wait < 100; wait++)
    {
        FILE* progress_file = fopen("/mnt/memproc/misc/procinfo/progress_percent.txt", "r");
        if (progress_file)
        {
            char bytes[16] = {0};
            if (fread(bytes, 1, 15, progress_file) > 0 && atoi(bytes) >= 100)
            {
                fclose(progress_file);
                break;
            }
            fclose(progress_file);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Read dtb.txt and collect possible DTBs
    std::vector<uint64_t> possible_dtbs;

    FILE* dtb_file = fopen("/mnt/memproc/misc/procinfo/dtb.txt", "r");
    if (!dtb_file)
    {
        printf("[!] Failed to open dtb.txt\n");

        terminate_memprocfs();
        return false;
    }

    char line[512];
    printf("[>] Parsing dtb.txt for PID %d and suspect DTBs...\n", process_id);

    while (fgets(line, sizeof(line), dtb_file))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        Info info = {};
        char name_buf[256] = {0};

        if (sscanf(line, "%x %d %llx %llx %255[^\n]",
                   &info.index, &info.process_id,
                   &info.dtb, &info.kernelAddr,
                   name_buf) >= 4)
        {
            strncpy(info.name, name_buf, sizeof(info.name) - 1);
            info.name[sizeof(info.name) - 1] = '\0';

            if (info.process_id == 0)
            {
                possible_dtbs.push_back(info.dtb);
                printf("[DBG] Suspect DTB (PID 0): 0x%llX\n", info.dtb);
            }

            // Check if name matches our process
            if (strlen(name_buf) > 0 &&
                (process_name.find(name_buf) != std::string::npos ||
                 strcasestr(name_buf, process_name.c_str()) != NULL))
            {
                possible_dtbs.push_back(info.dtb);
                printf("[DBG] Name match DTB (%s): 0x%llX\n", name_buf, info.dtb);
            }

            // Also check if this is our PID
            if (info.process_id == (uint32_t)process_id)
            {
                possible_dtbs.push_back(info.dtb);
                printf("[DBG] PID match DTB: 0x%llX\n", info.dtb);
            }
        }
    }
    fclose(dtb_file);

    printf("[>] Found %zu possible DTBs to try\n", possible_dtbs.size());

    // Try each DTB
    for (size_t i = 0; i < possible_dtbs.size(); i++)
    {
        ULONG64 dtb = possible_dtbs[i];
        printf("[>] Trying DTB 0x%llX...\n", dtb);

        VMMDLL_ConfigSet(hVMM, VMMDLL_OPT_PROCESS_DTB | process_id, dtb);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        VMMDLL_ConfigSet(hVMM, VMMDLL_OPT_REFRESH_ALL, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &module_entry, NULL))
        {
            printf("[+] Patched DTB: 0x%llX - Found %s!\n", dtb, process_name.c_str());
            VMMDLL_MemFree(module_entry);

            terminate_memprocfs();
            return true;
        }
    }

    printf("[-] Failed to patch DTB\n");

    terminate_memprocfs();
    return false;
} 
#endif

bool get_process_base_address(const std::string process_name, const uint32_t& process_id)
{
    PVMMDLL_MAP_MODULEENTRY pModuleEntryExplorer;

    bool result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &pModuleEntryExplorer, NULL);

    if (result) {
        process_size = pModuleEntryExplorer->cbImageSize;
        process_base_address = pModuleEntryExplorer->vaBase;
        VMMDLL_MemFree(pModuleEntryExplorer);
        return true;
    }

    // If not found, fix DTB and try again
    if (!FixCr3_1())
        return false;

    result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(process_name.c_str()), &pModuleEntryExplorer, NULL);

    if (result) {
        process_size = pModuleEntryExplorer->cbImageSize;
        process_base_address = pModuleEntryExplorer->vaBase;
        VMMDLL_MemFree(pModuleEntryExplorer);
        return true;
    }

    return false;
}

bool GetDLLModuleBase(const uint32_t& process_id, const std::string DLL_Name)
{
    PVMMDLL_MAP_MODULEENTRY pModuleEntryExplorer;

    bool result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(DLL_Name.c_str()), &pModuleEntryExplorer, VMMDLL_MODULE_FLAG_NORMAL);

    if (result) {
        DLL_base_address = pModuleEntryExplorer->vaBase;
        DLL_size = pModuleEntryExplorer->cbImageSize;
        VMMDLL_MemFree(pModuleEntryExplorer);

        printf("[+] DLL %s: Base=0x%llX, Size=0x%llX\n", DLL_Name.c_str(), DLL_base_address, DLL_size);
        return true;
    }

    // If not found, fix DTB and try again
    if (!FixCr3_1())
        return false;

    result = VMMDLL_Map_GetModuleFromNameU(hVMM, process_id, const_cast<char*>(DLL_Name.c_str()), &pModuleEntryExplorer, VMMDLL_MODULE_FLAG_NORMAL);

    if (result) {
        DLL_base_address = pModuleEntryExplorer->vaBase;
        DLL_size = pModuleEntryExplorer->cbImageSize;
        VMMDLL_MemFree(pModuleEntryExplorer);

        printf("[+] DLL %s: Base=0x%llX, Size=0x%llX\n", DLL_Name.c_str(), DLL_base_address, DLL_size);
        return true;
    }

    printf("[!] Failed to find %s\n", DLL_Name.c_str());
    return false;
}

#ifdef _WIN32
uintptr_t PatternScan1(void* module, const char* signature, const char* sectionName, int skip)
{
    static auto pattern_to_byte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
        };

    auto dosHeader = (PIMAGE_DOS_HEADER)module;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);
    auto patternBytes = pattern_to_byte(signature);
    auto s = patternBytes.size();
    auto d = patternBytes.data();
    int currentskip = 0;

    if (!sectionName) {
        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

        for (auto i = 0ul; i < sizeOfImage - s; ++i) {
            if (currentskip < skip) {
                currentskip++;
                continue;
            }

            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (scanBytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return (uintptr_t)&scanBytes[i];
            }
        }
    }
    else {
        auto sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
            if (strncmp(reinterpret_cast<const char*>(sectionHeader->Name), sectionName, IMAGE_SIZEOF_SHORT_NAME) == 0) {
                auto sectionStart = reinterpret_cast<std::uint8_t*>(module) + sectionHeader->VirtualAddress;
                auto sectionSize = sectionHeader->Misc.VirtualSize;

                for (auto j = 0ul; j < sectionSize - s; ++j) {
                    bool found = true;
                    for (auto k = 0ul; k < s; ++k) {
                        if (sectionStart[j + k] != d[k] && d[k] != -1) {
                            found = false;
                            break;
                        }
                    }
                    if (found) {
                        if (currentskip < skip) {
                            currentskip++;
                            continue;
                        }
                        return (uintptr_t)&sectionStart[j];
                    }
                }
                break; // Stop searching if section is found
            }
        }
    }
    return (uintptr_t)nullptr;
}
#endif

uintptr_t PatternScan(const char* signature)
{
    static auto pattern_to_byte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
        };

    uintptr_t moduleBase = DLL_base_address;
    size_t moduleSize = DLL_size;

    auto patternBytes = pattern_to_byte(signature);
    auto scanSize = patternBytes.size();
    auto patternData = patternBytes.data();

    // Read the entire module into buffer
    std::vector<uint8_t> moduleBuffer(moduleSize);
    if (!VMMDLL_MemReadEx(hVMM, process_id, moduleBase, moduleBuffer.data(), moduleSize, 0, VMMDLL_FLAG_NOCACHE)) {
        printf("[!] PatternScan: Failed to read DLL memory\n");
        return 0;
    }


    printf("[>] Scanning pattern in 0x%zX bytes...\n", moduleSize);

    for (size_t i = 0; i < moduleSize - scanSize; ++i)
    {
        bool found = true;

        for (size_t j = 0; j < scanSize; ++j)
        {
            // FIX: Compare against the actual buffer we read
            if (patternData[j] != -1 && patternData[j] != moduleBuffer[i + j])
            {
                found = false;
                break;
            }
        }

        if (found)
        {
            uintptr_t found_address = moduleBase + i;
            uintptr_t relative_address = found_address - DLL_base_address;

            printf("[+] Pattern found:\n");
            printf("    Absolute: 0x%llX\n", found_address);
            printf("    Relative: 0x%llX\n", relative_address);

            // Debug: show what we found
            printf("    Bytes at location: ");
            for (size_t k = 0; k < std::min(scanSize, size_t(16)); k++) {
                printf("%02X ", moduleBuffer[i + k]);
            }
            printf("\n");

            return found_address; // Return absolute for now
        }
    }

    printf("[-] Pattern not found in module\n");
    return 0;
}

std::vector<int> GetPidListFromName(std::string name)
{
    PVMMDLL_PROCESS_INFORMATION process_info = NULL;
    DWORD total_processes = 0;
    std::vector<int> list = { };

    if (!VMMDLL_ProcessGetInformationAll(hVMM, &process_info, &total_processes))
    {
        printf("[!] Failed to get process list\n");
        return list;
    }

    for (size_t i = 0; i < total_processes; i++)
    {
        auto process = process_info[i];
        if (strstr(process.szNameLong, name.c_str()))
            list.push_back(process.dwPID);
    }

    return list;
}

#ifdef _WIN32

bool DumpExe() {
    printf("[>] Creating memory-formatted executable dump...\n");

    if (!process_id || !process_base_address || !process_size)
    {
        printf("[!] Memory is not initialized.\n");
        return false;
    }

    printf("[>] Reading 0x%X bytes from 0x%llX...\n",
        process_size, process_base_address);

    std::vector<BYTE> memory_buffer(process_size);
    DWORD total_read = 0;

    // Read memory in chunks
    for (DWORD offset = 0; offset < process_size; offset += 0x1000)
    {
        DWORD to_read = (0x1000UL < (process_size - offset)) ? 0x1000UL : (process_size - offset);
        DWORD chunk_read = 0;

        VMMDLL_MemReadEx(hVMM, process_id, process_base_address + offset,
            memory_buffer.data() + offset, to_read, &chunk_read,
            VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
        total_read += chunk_read;
    }

    printf("[+] Read 0x%X bytes from memory\n", total_read);

    PIMAGE_DOS_HEADER pdos_header = (PIMAGE_DOS_HEADER)memory_buffer.data();

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("[!] Invalid DOS header (0x%04X)\n", pdos_header->e_magic);
        return false;
    }

    printf("[+] DOS header valid (MZ)\n");
    printf("[+] e_lfanew: 0x%X\n", pdos_header->e_lfanew);

    if (pdos_header->e_lfanew >= memory_buffer.size() - sizeof(DWORD))
    {
        printf("[!] e_lfanew out of bounds\n");
        return false;
    }

    PIMAGE_NT_HEADERS_WIN_UNION pnt_union = (PIMAGE_NT_HEADERS_WIN_UNION)
        (memory_buffer.data() + pdos_header->e_lfanew);

    if (pnt_union->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("[!] Invalid PE signature (0x%08X)\n", pnt_union->Signature);
        return false;
    }

    printf("[+] PE signature valid\n");

    // Check architecture using your union type
    PIMAGE_OPTIONAL_HEADER_WIN_UNION popt_union = (PIMAGE_OPTIONAL_HEADER_WIN_UNION)
        ((BYTE*)pnt_union + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));

    bool is_64bit = (popt_union->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

    printf("[+] Architecture: %s-bit\n", is_64bit ? "64" : "32");

    DWORD size_of_image = 0;
    DWORD size_of_headers = 0;
    WORD number_of_sections = 0;

    if (is_64bit)
    {
        size_of_image = pnt_union->Headers64.OptionalHeader.SizeOfImage;
        size_of_headers = pnt_union->Headers64.OptionalHeader.SizeOfHeaders;
        number_of_sections = pnt_union->Headers64.FileHeader.NumberOfSections;
    }
    else
    {
        size_of_image = pnt_union->Headers32.OptionalHeader.SizeOfImage;
        size_of_headers = pnt_union->Headers32.OptionalHeader.SizeOfHeaders;
        number_of_sections = pnt_union->Headers32.FileHeader.NumberOfSections;
    }

    printf("[+] Original SizeOfImage: 0x%X\n", size_of_image);
    printf("[+] SizeOfHeaders: 0x%X\n", size_of_headers);
    printf("[+] Number of sections: %d\n", number_of_sections);

    // Get first section header using your macros
    PIMAGE_SECTION_HEADER first_section = NULL;

    if (is_64bit)
    {
        first_section = (PIMAGE_SECTION_HEADER)((ULONG_PTR)&pnt_union->Headers64 +
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
            pnt_union->Headers64.FileHeader.SizeOfOptionalHeader);
    }
    else
    {
        first_section = (PIMAGE_SECTION_HEADER)((ULONG_PTR)&pnt_union->Headers32 +
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
            pnt_union->Headers32.FileHeader.SizeOfOptionalHeader);
    }

    // Fix section headers to point to memory layout
    for (WORD i = 0; i < number_of_sections; i++)
    {
        PIMAGE_SECTION_HEADER section = &first_section[i];

        printf("[+] Section %d: ", i + 1);
        // Print section name safely
        for (int j = 0; j < 8 && section->Name[j] != 0; j++) {
            printf("%c", section->Name[j]);
        }
        printf("\n");

        printf("    VirtualAddress:   0x%08X\n", section->VirtualAddress);
        printf("    VirtualSize:      0x%08X\n", section->Misc.VirtualSize);
        printf("    SizeOfRawData:    0x%08X", section->SizeOfRawData);

        // Ensure SizeOfRawData is at least VirtualSize
        if (section->SizeOfRawData < section->Misc.VirtualSize)
        {
            section->SizeOfRawData = section->Misc.VirtualSize;
            printf(" -> 0x%08X", section->SizeOfRawData);
        }
        printf("\n");

        printf("    PointerToRawData: 0x%08X", section->PointerToRawData);

        // Set file offset to match memory offset
        section->PointerToRawData = section->VirtualAddress;

        printf(" -> 0x%08X\n", section->PointerToRawData);
    }

    // Update SizeOfImage to actual memory size
    if (is_64bit)
    {
        pnt_union->Headers64.OptionalHeader.SizeOfImage = process_size;
        pnt_union->Headers64.OptionalHeader.CheckSum = 0; // Clear checksum
    }
    else
    {
        pnt_union->Headers32.OptionalHeader.SizeOfImage = process_size;
        pnt_union->Headers32.OptionalHeader.CheckSum = 0;
    }

    printf("[+] Updated SizeOfImage: 0x%X -> 0x%X\n", size_of_image, process_size);

    char filename[MAX_PATH];
    const char* arch_suffix = is_64bit ? "x64_memdump" : "x86_memdump";

    sprintf_s(filename, MAX_PATH, "%s_%s.exe", process_name.c_str(), arch_suffix);

    printf("\n[>] Saving memory-formatted executable to %s...\n", filename);

    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("[!] Failed to create file (Error: %d)\n", GetLastError());
        return false;
    }

    DWORD bytes_written = 0;
    WriteFile(hFile, memory_buffer.data(), (DWORD)memory_buffer.size(), &bytes_written, NULL);
    CloseHandle(hFile);

    printf("[+] Saved: %lu bytes (0x%lX)\n", bytes_written, bytes_written);

    printf("\n[>] Verifying memory dump...\n");

    FILE* verify_file = fopen(filename, "rb");
    if (verify_file)
    {
        BYTE header_check[0x400];
        size_t bytes_read = fread(header_check, 1, sizeof(header_check), verify_file);
        fclose(verify_file);

        if (bytes_read >= sizeof(IMAGE_DOS_HEADER))
        {
            PIMAGE_DOS_HEADER check_dos = (PIMAGE_DOS_HEADER)header_check;

            if (check_dos->e_magic == IMAGE_DOS_SIGNATURE)
            {
                printf("[+] DOS header: OK\n");

                if (check_dos->e_lfanew < bytes_read - sizeof(DWORD))
                {
                    DWORD* pe_sig = (DWORD*)(header_check + check_dos->e_lfanew);

                    if (*pe_sig == IMAGE_NT_SIGNATURE)
                    {
                        printf("[+] PE signature: OK\n");
                        printf("[+] Memory dump appears valid!\n");
                    }
                }
            }
        }
    }

    printf("\n[+] MEMORY DUMP COMPLETE\n");
    printf("    File: %s\n", filename);
    printf("    Size: %lu bytes (full memory image)\n", bytes_written);
    printf("    Layout: Memory layout preserved\n");
    printf("    State: Live memory state retained\n");
    printf("    Note: This is a memory dump formatted as an executable\n");

    return true;
}

bool DumpDLL()
{
    printf("[>] Creating memory-formatted DLL dump...\n");

    if (!process_id || !DLL_base_address || !DLL_size)
    {
        printf("[!] DLL memory is not initialized.\n");
        return false;
    }

    printf("[>] Reading 0x%X bytes from DLL at 0x%llX...\n",
        DLL_size, DLL_base_address);

    std::vector<BYTE> memory_buffer(DLL_size);
    DWORD total_read = 0;

    for (DWORD offset = 0; offset < DLL_size; offset += 0x1000)
    {
        DWORD to_read = (0x1000UL < (DLL_size - offset)) ? 0x1000UL : (DLL_size - offset);
        DWORD chunk_read = 0;

        VMMDLL_MemReadEx(hVMM, process_id, DLL_base_address + offset,
            memory_buffer.data() + offset, to_read, &chunk_read,
            VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
        total_read += chunk_read;
    }

    printf("[+] Read 0x%X bytes from DLL memory\n", total_read);

    PIMAGE_DOS_HEADER pdos_header = (PIMAGE_DOS_HEADER)memory_buffer.data();

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("[!] Invalid DLL DOS header (0x%04X)\n", pdos_header->e_magic);
        return false;
    }

    printf("[+] DLL DOS header valid\n");
    printf("[+] e_lfanew: 0x%X\n", pdos_header->e_lfanew);

    if (pdos_header->e_lfanew >= memory_buffer.size() - sizeof(DWORD))
    {
        printf("[!] e_lfanew out of bounds\n");
        return false;
    }

    PIMAGE_NT_HEADERS_WIN_UNION pnt_union = (PIMAGE_NT_HEADERS_WIN_UNION)
        (memory_buffer.data() + pdos_header->e_lfanew);

    if (pnt_union->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("[!] Invalid DLL PE signature (0x%08X)\n", pnt_union->Signature);
        return false;
    }

    printf("[+] DLL PE signature valid\n");

    // Check architecture
    PIMAGE_OPTIONAL_HEADER_WIN_UNION popt_union = (PIMAGE_OPTIONAL_HEADER_WIN_UNION)
        ((BYTE*)pnt_union + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));

    bool is_64bit = (popt_union->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

    printf("[+] Architecture: %s-bit\n", is_64bit ? "64" : "32");

    // Check DLL characteristics
    WORD characteristics = is_64bit ?
        pnt_union->Headers64.FileHeader.Characteristics :
        pnt_union->Headers32.FileHeader.Characteristics;

    bool is_dll = (characteristics & IMAGE_FILE_DLL) != 0;
    printf("[+] DLL flag: %s\n", is_dll ? "YES (valid DLL)" : "NO (may be memory region)");

    WORD number_of_sections = is_64bit ?
        pnt_union->Headers64.FileHeader.NumberOfSections :
        pnt_union->Headers32.FileHeader.NumberOfSections;

    printf("[+] Number of sections: %d\n", number_of_sections);

    // Get first section header
    PIMAGE_SECTION_HEADER first_section = NULL;

    if (is_64bit)
    {
        first_section = (PIMAGE_SECTION_HEADER)((ULONG_PTR)&pnt_union->Headers64 +
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
            pnt_union->Headers64.FileHeader.SizeOfOptionalHeader);
    }
    else
    {
        first_section = (PIMAGE_SECTION_HEADER)((ULONG_PTR)&pnt_union->Headers32 +
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
            pnt_union->Headers32.FileHeader.SizeOfOptionalHeader);
    }

    // Update section headers for memory layout
    for (WORD i = 0; i < number_of_sections; i++)
    {
        PIMAGE_SECTION_HEADER section = &first_section[i];

        printf("[+] Section %d: ", i + 1);
        // Print section name safely
        for (int j = 0; j < 8 && section->Name[j] != 0; j++) {
            printf("%c", section->Name[j]);
        }
        printf("\n");

        // Ensure SizeOfRawData is sufficient
        if (section->SizeOfRawData < section->Misc.VirtualSize)
        {
            section->SizeOfRawData = section->Misc.VirtualSize;
        }

        // Set file offset to match memory offset
        section->PointerToRawData = section->VirtualAddress;

        // Check for important DLL sections
        bool is_export = false;
        bool is_reloc = false;
        for (int j = 0; j < 8; j++) {
            if (section->Name[j] == '.') {
                if (j < 7 && section->Name[j + 1] == 'e' && section->Name[j + 2] == 'd' && section->Name[j + 3] == 'a') {
                    is_export = true;
                }
                else if (j < 7 && section->Name[j + 1] == 'r' && section->Name[j + 2] == 'e' && section->Name[j + 3] == 'l') {
                    is_reloc = true;
                }
            }
        }

        if (is_export) printf("    [*] Export section\n");
        if (is_reloc) printf("    [*] Relocation section\n");
    }

    if (is_64bit)
    {
        pnt_union->Headers64.OptionalHeader.SizeOfImage = DLL_size;
        pnt_union->Headers64.OptionalHeader.CheckSum = 0;

        // Ensure DLL flag is set
        if (!is_dll)
        {
            pnt_union->Headers64.FileHeader.Characteristics |= IMAGE_FILE_DLL;
            printf("[+] Added DLL characteristics flag\n");
        }
    }
    else
    {
        pnt_union->Headers32.OptionalHeader.SizeOfImage = DLL_size;
        pnt_union->Headers32.OptionalHeader.CheckSum = 0;

        if (!is_dll)
        {
            pnt_union->Headers32.FileHeader.Characteristics |= IMAGE_FILE_DLL;
            printf("[+] Added DLL characteristics flag\n");
        }
    }

    char filename[MAX_PATH];
    const char* arch_suffix = is_64bit ? "x64_memdump" : "x86_memdump";

    sprintf_s(filename, MAX_PATH, "%s_%s.dll", DLL_Name.c_str(), arch_suffix);

    printf("\n[>] Saving memory-formatted DLL to %s...\n", filename);

    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("[!] Failed to create DLL file (Error: %d)\n", GetLastError());
        return false;
    }

    DWORD bytes_written = 0;
    WriteFile(hFile, memory_buffer.data(), (DWORD)memory_buffer.size(), &bytes_written, NULL);
    CloseHandle(hFile);

    printf("[+] Saved: %lu bytes (0x%lX)\n", bytes_written, bytes_written);

    printf("\n[>] Verifying DLL memory dump...\n");

    FILE* verify_file = fopen(filename, "rb");
    if (verify_file)
    {
        BYTE header_check[0x400];
        size_t bytes_read = fread(header_check, 1, sizeof(header_check), verify_file);
        fclose(verify_file);

        if (bytes_read >= sizeof(IMAGE_DOS_HEADER))
        {
            PIMAGE_DOS_HEADER check_dos = (PIMAGE_DOS_HEADER)header_check;

            if (check_dos->e_magic == IMAGE_DOS_SIGNATURE)
            {
                printf("[+] DOS header: OK\n");

                if (check_dos->e_lfanew < bytes_read - sizeof(DWORD))
                {
                    DWORD* pe_sig = (DWORD*)(header_check + check_dos->e_lfanew);

                    if (*pe_sig == IMAGE_NT_SIGNATURE)
                    {
                        printf("[+] PE signature: OK\n");

                        // Check DLL flag
                        PIMAGE_FILE_HEADER file_header = (PIMAGE_FILE_HEADER)((BYTE*)pe_sig + sizeof(DWORD));
                        if (file_header->Characteristics & IMAGE_FILE_DLL)
                        {
                            printf("[+] DLL flag: YES\n");
                        }
                        printf("[+] Memory dump appears valid!\n");
                    }
                }
            }
        }
    }

    printf("\n[+] DLL MEMORY DUMP COMPLETE\n");
    printf("    File: %s\n", filename);
    printf("    Size: %lu bytes (full DLL memory image)\n", bytes_written);
    printf("    Layout: Memory layout preserved\n");
    printf("    State: Live memory state retained\n");
    printf("    Note: This is a memory dump formatted as a DLL\n");

    return true;
}

#endif

#ifdef LINUX

bool DebugMemoryRead(ULONGLONG address, DWORD size, const std::string& description)
{
    printf("\n[>] DEBUG: Reading memory at 0x%llX (%s)...\n", address, description.c_str());

    BYTE buffer[256];
    DWORD bytes_read = 0;

    if (!VMMDLL_MemReadEx(hVMM, process_id, address, buffer, sizeof(buffer), &bytes_read, 0))
    {
        printf("[!] Failed to read memory\n");
        return false;
    }

    printf("[+] Read %u bytes\n", bytes_read);
    printf("[+] First 64 bytes (hex):\n");

    for (DWORD i = 0; i < std::min(bytes_read, (DWORD)64); i += 16)
    {
        printf("    0x%04X: ", i);
        for (DWORD j = 0; j < 16; j++)
        {
            if (i + j < bytes_read)
                printf("%02X ", buffer[i + j]);
            else
                printf("   ");
        }
        printf(" ");
        for (DWORD j = 0; j < 16; j++)
        {
            if (i + j < bytes_read)
            {
                BYTE c = buffer[i + j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            else
            {
                printf(" ");
            }
        }
        printf("\n");
    }

    // Check for MZ
    if (bytes_read >= 2)
    {
        printf("[+] First 2 bytes: 0x%02X 0x%02X = ", buffer[0], buffer[1]);
        if (buffer[0] == 0x4D && buffer[1] == 0x5A)
            printf("MZ (VALID)\n");
        else
            printf("NOT MZ\n");
    }

    // Check e_lfanew if we have enough data
    if (bytes_read >= 0x40)
    {
        LONG e_lfanew = *reinterpret_cast<LONG*>(buffer + 0x3C);
        printf("[+] e_lfanew at offset 0x3C: 0x%X\n", e_lfanew);

        // Try to read NT signature
        if (e_lfanew > 0 && e_lfanew < 1000)
        {
            ULONGLONG nt_addr = address + e_lfanew;
            printf("[+] NT signature should be at: 0x%llX\n", nt_addr);

            BYTE nt_sig[4];
            DWORD nt_bytes = 0;
            if (VMMDLL_MemReadEx(hVMM, process_id, nt_addr, nt_sig, 4, &nt_bytes, 0))
            {
                DWORD signature = *reinterpret_cast<DWORD*>(nt_sig);
                printf("[+] NT signature: 0x%08X = ", signature);
                if (signature == IMAGE_NT_SIGNATURE)
                    printf("PE\\0\\0 (VALID)\n");
                else
                    printf("INVALID\n");
            }
        }
    }

    return true;
}

void DebugAllModules()
{
    PVMMDLL_MAP_MODULE pModuleMap = NULL;
    if (!VMMDLL_Map_GetModuleU(hVMM, process_id, &pModuleMap, NULL)) {
        printf("[!] Failed to get module list\n");
        return;
    }

    printf("\n[>] ALL MODULES IN PROCESS %d:\n", process_id);
    printf("=================================================================\n");

    for (DWORD i = 0; i < pModuleMap->cMap; i++) {
        PVMMDLL_MAP_MODULEENTRY pEntry = pModuleMap->pMap + i;

        printf("[%3d] 0x%-14llX 0x%-12llX %s\n",
               i,
               pEntry->vaBase,
               pEntry->cbImageSize,
               pEntry->uszText);
    }

    printf("[+] Total modules: %d\n", pModuleMap->cMap);

    // Find potential game executables
    printf("\n[>] POTENTIAL GAME EXECUTABLES:\n");
    printf("=================================================================\n");

    for (DWORD i = 0; i < pModuleMap->cMap; i++) {
        PVMMDLL_MAP_MODULEENTRY pEntry = pModuleMap->pMap + i;
        std::string name = pEntry->uszText;

        // Look for game-related names
        if (name.find(".exe") != std::string::npos ||
            name.find("r5apex") != std::string::npos ||
            name.find("apex") != std::string::npos ||
            name.find("R5") != std::string::npos ||
            pEntry->cbImageSize > 0x1000000) {  // > 16MB

            printf("  -> 0x%-14llX 0x%-12llX (%llu MB) %s\n",
                   pEntry->vaBase,
                   pEntry->cbImageSize,
                   pEntry->cbImageSize / (1024 * 1024),
                   pEntry->uszText);
        }
    }

    VMMDLL_MemFree(pModuleMap);
}

bool DumpExe()
{
    printf("[>] Creating executable dump...\n");

    if (!process_id || !process_base_address || !process_size)
    {
        printf("[!] Memory is not initialized.\n");
        return false;
    }

    printf("[>] Reading 0x%X bytes from 0x%llX...\n", process_size, process_base_address);

    std::vector<uint8_t> memory_buffer(process_size, 0);
    DWORD total_read = 0;
    DWORD failed_reads = 0;

    for (DWORD offset = 0; offset < process_size; offset += 0x1000)
    {
        DWORD to_read = (0x1000UL < (process_size - offset)) ? 0x1000UL : (process_size - offset);
        DWORD chunk_read = 0;

        if (!VMMDLL_MemReadEx(hVMM, process_id, process_base_address + offset,
                             memory_buffer.data() + offset, to_read, &chunk_read,
                             VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL))
        {
            printf("[!] Failed to read at offset 0x%X\n", offset);
            failed_reads++;
        }
        total_read += chunk_read;

        // Show progress
        if (offset % 0x10000 == 0 && offset > 0)
        {
            printf("[>] Progress: 0x%X/0x%X bytes\n", offset, process_size);
        }
    }

    printf("[+] Read 0x%X of 0x%X bytes (%d failed pages)\n", total_read, process_size, failed_reads);

    if (failed_reads > 10)
    {
        printf("[!] Warning: Many read failures, image may be incomplete\n");
    }

    PIMAGE_DOS_HEADER pdos_header = (PIMAGE_DOS_HEADER)memory_buffer.data();

    // Debug first few bytes
    printf("[+] First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", memory_buffer[i]);
    }
    printf("\n");

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("[!] Invalid DOS header (not MZ: 0x%04X)\n", pdos_header->e_magic);
        return false;
    }

    printf("[+] DOS header valid (MZ)\n");
    printf("[+] e_lfanew: 0x%X\n", pdos_header->e_lfanew);

    if (pdos_header->e_lfanew + sizeof(DWORD) > total_read)
    {
        printf("[!] e_lfanew (0x%X) out of bounds (buffer: 0x%X)\n",
               pdos_header->e_lfanew, total_read);
        return false;
    }

    PIMAGE_NT_HEADERS_UNION pnt_union = (PIMAGE_NT_HEADERS_UNION)(memory_buffer.data() + pdos_header->e_lfanew);

    if (pnt_union->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("[!] Invalid PE signature: 0x%08X\n", pnt_union->Signature);
        printf("[!] Bytes at e_lfanew: ");
        for (int i = 0; i < 8; i++) {
            printf("%02X ", memory_buffer[pdos_header->e_lfanew + i]);
        }
        printf("\n");
        return false;
    }

    printf("[+] PE signature valid\n");
    printf("[+] Number of sections: %d\n", pnt_union->Common.FileHeader.NumberOfSections);

    BYTE* optional_header_ptr = (BYTE*)&pnt_union->Common.FileHeader + sizeof(IMAGE_FILE_HEADER);
    PIMAGE_OPTIONAL_HEADER_UNION popt_union = (PIMAGE_OPTIONAL_HEADER_UNION)optional_header_ptr;

    // Check magic properly
    WORD magic = popt_union->OptionalHeader32.Magic;
    bool is_64bit = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    printf("[+] Architecture: %s-bit\n", is_64bit ? "64" : "32");

    DWORD optional_header_size = pnt_union->Common.FileHeader.SizeOfOptionalHeader;
    WORD number_of_sections = pnt_union->Common.FileHeader.NumberOfSections;

    DWORD size_of_headers = 0;
    DWORD size_of_image = 0;
    DWORD file_alignment = 0x200;
    DWORD section_alignment = 0x1000;
    ULONGLONG image_base = 0;

    if (is_64bit)
    {
        size_of_headers = popt_union->OptionalHeader64.SizeOfHeaders;
        size_of_image = popt_union->OptionalHeader64.SizeOfImage;
        file_alignment = popt_union->OptionalHeader64.FileAlignment;
        section_alignment = popt_union->OptionalHeader64.SectionAlignment;
        image_base = popt_union->OptionalHeader64.ImageBase;
    }
    else
    {
        size_of_headers = popt_union->OptionalHeader32.SizeOfHeaders;
        size_of_image = popt_union->OptionalHeader32.SizeOfImage;
        file_alignment = popt_union->OptionalHeader32.FileAlignment;
        section_alignment = popt_union->OptionalHeader32.SectionAlignment;
        image_base = popt_union->OptionalHeader32.ImageBase;
    }

    printf("[+] Original SizeOfImage: 0x%X\n", size_of_image);
    printf("[+] SizeOfHeaders: 0x%X\n", size_of_headers);

    if (file_alignment == 0) file_alignment = 0x200;
    if (section_alignment == 0) section_alignment = 0x1000;

    PIMAGE_SECTION_HEADER first_section = IMAGE_FIRST_SECTION_UNION(pnt_union);

    printf("[+] Fixing section headers for memory layout...\n");

    for (WORD i = 0; i < number_of_sections; i++)
    {
        PIMAGE_SECTION_HEADER section = &first_section[i];

        printf("[+] Section %d: ", i + 1);
        // Print section name
        for (int j = 0; j < 8 && section->Name[j] != 0; j++) {
            printf("%c", section->Name[j]);
        }
        printf("\n");

        printf("    VirtualAddress:   0x%08X\n", section->VirtualAddress);
        printf("    VirtualSize:      0x%08X\n", section->Misc.VirtualSize);

        printf("    SizeOfRawData:    0x%08X", section->SizeOfRawData);
        // Ensure SizeOfRawData is at least VirtualSize
        if (section->SizeOfRawData < section->Misc.VirtualSize)
        {
            section->SizeOfRawData = section->Misc.VirtualSize;
            printf(" -> 0x%08X", section->SizeOfRawData);
        }
        printf("\n");

        printf("    PointerToRawData: 0x%08X", section->PointerToRawData);
        // Set file offset to match memory offset
        section->PointerToRawData = section->VirtualAddress;
        printf(" -> 0x%08X\n", section->PointerToRawData);
    }

    if (is_64bit)
    {
        popt_union->OptionalHeader64.SizeOfImage = process_size;
        popt_union->OptionalHeader64.CheckSum = 0;
    }
    else
    {
        popt_union->OptionalHeader32.SizeOfImage = process_size;
        popt_union->OptionalHeader32.CheckSum = 0;
    }

    printf("[+] Updated SizeOfImage: 0x%X -> 0x%X\n", size_of_image, process_size);

    char filename[256];
    const char* arch_suffix = is_64bit ? "x64_memdump" : "x86_memdump";
    snprintf(filename, sizeof(filename), "%s_%s.exe",
             process_name.c_str(), arch_suffix);

    printf("\n[>] Saving memory-formatted executable to %s...\n", filename);

    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        printf("[!] Failed to create file\n");
        return false;
    }

    size_t written = fwrite(memory_buffer.data(), 1, memory_buffer.size(), file);
    fclose(file);

    printf("[+] Saved: %zu bytes (0x%zX)\n", written, written);

    printf("\n[>] Verifying memory dump...\n");

    FILE* verify = fopen(filename, "rb");
    if (verify)
    {
        IMAGE_DOS_HEADER verify_dos;
        size_t bytes_read = fread(&verify_dos, 1, sizeof(verify_dos), verify);

        if (bytes_read >= sizeof(IMAGE_DOS_HEADER) && verify_dos.e_magic == IMAGE_DOS_SIGNATURE)
        {
            printf("[+] DOS header: OK\n");

            fseek(verify, verify_dos.e_lfanew, SEEK_SET);
            DWORD verify_pe;
            fread(&verify_pe, 1, sizeof(verify_pe), verify);

            if (verify_pe == IMAGE_NT_SIGNATURE)
            {
                printf("[+] PE signature: OK\n");
                printf("[+] Memory dump appears valid!\n");
            }
            else
            {
                printf("[!] PE signature invalid\n");
            }
        }
        else
        {
            printf("[!] DOS header invalid\n");
        }
        fclose(verify);
    }
    else
    {
        printf("[!] Could not open file for verification\n");
    }

    printf("\n[+] MEMORY DUMP COMPLETE\n");
    printf("    File: %s\n", filename);
    printf("    Size: %zu bytes (full memory image)\n", written);
    printf("    Layout: Memory layout preserved\n");
    printf("    State: Live memory state retained\n");
    printf("    Note: This is a memory dump formatted as an executable\n");

    return true;
}

bool DumpDLL()
{
    printf("[>] Creating memory-formatted DLL dump...\n");

    if (!process_id || !DLL_base_address || !DLL_size)
    {
        printf("[!] DLL memory is not initialized.\n");
        return false;
    }

    printf("[>] Reading 0x%X bytes from DLL at 0x%llX...\n", DLL_size, DLL_base_address);

    std::vector<uint8_t> memory_buffer(DLL_size, 0);
    DWORD total_read = 0;
    DWORD failed_reads = 0;

    for (DWORD offset = 0; offset < DLL_size; offset += 0x1000)
    {
        DWORD to_read = (0x1000UL < (DLL_size - offset)) ? 0x1000UL : (DLL_size - offset);
        DWORD chunk_read = 0;

        if (!VMMDLL_MemReadEx(hVMM, process_id, DLL_base_address + offset,
                             memory_buffer.data() + offset, to_read, &chunk_read,
                             VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL))
        {
            printf("[!] Failed to read DLL at offset 0x%X\n", offset);
            failed_reads++;
        }
        total_read += chunk_read;

        if (offset % 0x10000 == 0 && offset > 0)
        {
            printf("[>] Progress: 0x%X/0x%X bytes\n", offset, DLL_size);
        }
    }

    printf("[+] Read 0x%X of 0x%X bytes (%d failed pages)\n", total_read, DLL_size, failed_reads);

    if (failed_reads > 10)
    {
        printf("[!] Warning: Many read failures, DLL may be incomplete\n");
    }

    PIMAGE_DOS_HEADER pdos_header = (PIMAGE_DOS_HEADER)memory_buffer.data();

    if (pdos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        printf("[!] Invalid DLL DOS header (not MZ: 0x%04X)\n", pdos_header->e_magic);
        return false;
    }

    printf("[+] DLL DOS header valid\n");
    printf("[+] e_lfanew: 0x%X\n", pdos_header->e_lfanew);

    if (pdos_header->e_lfanew + sizeof(DWORD) > total_read)
    {
        printf("[!] e_lfanew (0x%X) out of bounds (buffer: 0x%X)\n",
               pdos_header->e_lfanew, total_read);
        return false;
    }

    PIMAGE_NT_HEADERS_UNION pnt_union = (PIMAGE_NT_HEADERS_UNION)(memory_buffer.data() + pdos_header->e_lfanew);

    if (pnt_union->Signature != IMAGE_NT_SIGNATURE)
    {
        printf("[!] Invalid DLL PE signature: 0x%08X\n", pnt_union->Signature);
        return false;
    }

    printf("[+] DLL PE signature valid\n");

    BYTE* optional_header_ptr = (BYTE*)&pnt_union->Common.FileHeader + sizeof(IMAGE_FILE_HEADER);
    PIMAGE_OPTIONAL_HEADER_UNION popt_union = (PIMAGE_OPTIONAL_HEADER_UNION)optional_header_ptr;

    WORD magic = popt_union->OptionalHeader32.Magic;
    bool is_64bit = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    printf("[+] Architecture: %s-bit\n", is_64bit ? "64" : "32");

    WORD number_of_sections = pnt_union->Common.FileHeader.NumberOfSections;

    WORD characteristics = pnt_union->Common.FileHeader.Characteristics;
    bool is_dll = (characteristics & IMAGE_FILE_DLL) != 0;
    printf("[+] DLL flag: %s\n", is_dll ? "YES (valid DLL)" : "NO (may be memory region)");

    printf("[+] Number of sections: %d\n", number_of_sections);

    PIMAGE_SECTION_HEADER first_section = IMAGE_FIRST_SECTION_UNION(pnt_union);

    printf("[+] Fixing DLL section headers for memory layout...\n");

    for (WORD i = 0; i < number_of_sections; i++)
    {
        PIMAGE_SECTION_HEADER section = &first_section[i];

        printf("[+] Section %d: ", i + 1);
        // Print section name
        for (int j = 0; j < 8 && section->Name[j] != 0; j++) {
            printf("%c", section->Name[j]);
        }
        printf("\n");

        printf("    VirtualAddress:   0x%08X\n", section->VirtualAddress);

        // Ensure SizeOfRawData is sufficient
        printf("    SizeOfRawData:    0x%08X", section->SizeOfRawData);
        if (section->SizeOfRawData < section->Misc.VirtualSize)
        {
            section->SizeOfRawData = section->Misc.VirtualSize;
            printf(" -> 0x%08X", section->SizeOfRawData);
        }
        printf("\n");

        printf("    PointerToRawData: 0x%08X", section->PointerToRawData);
        // Set file offset to match memory offset
        section->PointerToRawData = section->VirtualAddress;
        printf(" -> 0x%08X\n", section->PointerToRawData);

        bool is_export = false;
        bool is_reloc = false;
        for (int j = 0; j < 8; j++) {
            if (section->Name[j] == '.') {
                if (j < 7 && section->Name[j+1] == 'e' && section->Name[j+2] == 'd' && section->Name[j+3] == 'a') {
                    is_export = true;
                } else if (j < 7 && section->Name[j+1] == 'r' && section->Name[j+2] == 'e' && section->Name[j+3] == 'l') {
                    is_reloc = true;
                }
            }
        }

        if (is_export) printf("    [*] Export section\n");
        if (is_reloc) printf("    [*] Relocation section\n");
    }

    if (is_64bit)
    {
        popt_union->OptionalHeader64.SizeOfImage = DLL_size;
        popt_union->OptionalHeader64.CheckSum = 0;

        // Ensure DLL flag is set
        if (!is_dll)
        {
            pnt_union->Common.FileHeader.Characteristics |= IMAGE_FILE_DLL;
            printf("[+] Added DLL characteristics flag\n");
        }
    }
    else
    {
        popt_union->OptionalHeader32.SizeOfImage = DLL_size;
        popt_union->OptionalHeader32.CheckSum = 0;

        if (!is_dll)
        {
            pnt_union->Common.FileHeader.Characteristics |= IMAGE_FILE_DLL;
            printf("[+] Added DLL characteristics flag\n");
        }
    }

    char filename[256];
    const char* arch_suffix = is_64bit ? "x64_memdump" : "x86_memdump";
    snprintf(filename, sizeof(filename), "%s_%s.dll",
             DLL_Name.c_str(), arch_suffix);

    printf("\n[>] Saving memory-formatted DLL to %s...\n", filename);

    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        printf("[!] Failed to create DLL file\n");
        return false;
    }

    size_t written = fwrite(memory_buffer.data(), 1, memory_buffer.size(), file);
    fclose(file);

    printf("[+] Saved: %zu bytes (0x%zX)\n", written, written);

    printf("\n[>] Verifying DLL memory dump...\n");

    FILE* verify = fopen(filename, "rb");
    if (verify)
    {
        IMAGE_DOS_HEADER verify_dos;
        size_t bytes_read = fread(&verify_dos, 1, sizeof(verify_dos), verify);

        if (bytes_read >= sizeof(IMAGE_DOS_HEADER) && verify_dos.e_magic == IMAGE_DOS_SIGNATURE)
        {
            printf("[+] DOS header: OK\n");

            fseek(verify, verify_dos.e_lfanew, SEEK_SET);
            DWORD verify_pe;
            fread(&verify_pe, 1, sizeof(verify_pe), verify);

            if (verify_pe == IMAGE_NT_SIGNATURE)
            {
                printf("[+] PE signature: OK\n");

                // Check DLL flag
                fseek(verify, verify_dos.e_lfanew + sizeof(DWORD), SEEK_SET);
                IMAGE_FILE_HEADER file_header;
                fread(&file_header, 1, sizeof(file_header), verify);

                if (file_header.Characteristics & IMAGE_FILE_DLL)
                {
                    printf("[+] DLL flag: YES\n");
                }
                printf("[+] Memory dump appears valid!\n");
            }
        }
        fclose(verify);
    }

    // Run file command to verify
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "file '%s' 2>/dev/null", filename);
    printf("[>] Running file command: ");
    system(cmd);

    printf("\n[+] DLL MEMORY DUMP COMPLETE\n");
    printf("    File: %s\n", filename);
    printf("    Size: %zu bytes (full DLL memory image)\n", written);
    printf("    Layout: Memory layout preserved\n");
    printf("    State: Live memory state retained\n");
    printf("    Note: This is a memory dump formatted as a DLL\n");

    return true;
}


#endif

/*
const char* LPWSTR_TO_CC(LPWSTR in)
{
    char buffer[500];
    wcstombs(buffer, in, 500);

    return buffer;
}
*/

LPSTR CC_TO_LPSTR(const char* in)
{
    LPSTR out = new char[strlen(in) + 1];
    #ifdef _WIN32
    strcpy_s(out, strlen(in) + 1, in);
    #endif
    #ifdef LINUX
    strcpy(out, in);
    #endif

    return out;
}

std::string QueryValue(const char* path, e_registry_type type)
{
    if (!hVMM)
        return "";

    BYTE buffer[0x128];
    DWORD _type = (DWORD)type;
    DWORD size = sizeof(buffer);

    if (!VMMDLL_WinReg_QueryValueExU(hVMM, CC_TO_LPSTR(path), &_type, buffer, &size))
    {
        printf("[!] failed QueryValueExU call\n");
        return nullptr;
    }

    std::wstring wstr = std::wstring((wchar_t*)buffer);
    return std::string(wstr.begin(), wstr.end());
}

bool InitKeyboard()
{
    std::string win = QueryValue("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\CurrentBuild", e_registry_type::sz);
    int Winver = 0;
    if (!win.empty())
        Winver = std::stoi(win);
    else
        return false;

    win_logon_pid = get_process_id("winlogon.exe");
    if (Winver > 22000)
    {
        auto pids = GetPidListFromName("csrss.exe");
        for (size_t i = 0; i < pids.size(); i++)
        {
            pid = pids[i];
            uintptr_t tmp = VMMDLL_ProcessGetModuleBaseU(hVMM, pid, (LPSTR)"win32ksgd.sys");
            uintptr_t g_session_global_slots = tmp + 0x3110;
            uintptr_t user_session_state = dma_read<uintptr_t>(dma_read<uintptr_t>(dma_read<uintptr_t>(g_session_global_slots, pid), pid), pid);
            gafAsyncKeyStateExport = user_session_state + 0x3690;
            if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
                break;
        }
        if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
            return true;
        return false;
    }
    else
    {
        PVMMDLL_MAP_EAT eat_map = NULL;
        PVMMDLL_MAP_EATENTRY eat_map_entry;
        bool result = VMMDLL_Map_GetEATU(hVMM, get_process_id("winlogon.exe") | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, (LPSTR)"win32kbase.sys", &eat_map);
        if (!result)
            return false;

        if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
        {
            VMMDLL_MemFree(eat_map);
            eat_map_entry = NULL;
            return false;
        }

        for (int i = 0; i < eat_map->cMap; i++)
        {
            eat_map_entry = eat_map->pMap + i;
            if (strcmp(eat_map_entry->uszFunction, "gafAsyncKeyState") == 0)
            {
                gafAsyncKeyStateExport = eat_map_entry->vaFunction;

                break;
            }
        }

        VMMDLL_MemFree(eat_map);
        eat_map = NULL;
        if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
            return true;
        return false;
    }
}

void UpdateKeys()
{
    uint8_t previous_key_state_bitmap[64] = { 0 };
    memcpy(previous_key_state_bitmap, state_bitmap, 64);

    VMMDLL_MemReadEx(hVMM, win_logon_pid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, gafAsyncKeyStateExport, (PBYTE)&state_bitmap, 64, NULL, VMMDLL_FLAG_NOCACHE);
    for (int vk = 0; vk < 256; ++vk)
        if ((state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) && !(previous_key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2))
            previous_state_bitmap[vk / 8] |= 1 << vk % 8;
}

bool IsKeyDown(uint32_t virtual_key_code)
{
    if (gafAsyncKeyStateExport < 0x7FFFFFFFFFFF)
        return false;
    if (std::chrono::system_clock::now() - start > std::chrono::milliseconds(5))
    {
        UpdateKeys();
        start = std::chrono::system_clock::now();
    }
    return state_bitmap[(virtual_key_code * 2 / 8)] & 1 << virtual_key_code % 4 * 2;
}

