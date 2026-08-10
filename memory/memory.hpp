#pragma once
#define NOMINMAX
#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>

typedef union _IMAGE_NT_HEADERS_WIN_UNION {
    DWORD Signature;
    IMAGE_NT_HEADERS32 Headers32;
    IMAGE_NT_HEADERS64 Headers64;
} IMAGE_NT_HEADERS_WIN_UNION, * PIMAGE_NT_HEADERS_WIN_UNION;

typedef union _IMAGE_OPTIONAL_HEADER_WIN_UNION {
    IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
} IMAGE_OPTIONAL_HEADER_WIN_UNION, * PIMAGE_OPTIONAL_HEADER_WIN_UNION;

#elif defined (__linux__)
#define LINUX
#include <filesystem>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>
#endif

#include <string.h>
#include <string_view>
#include <memory>
#include <fstream>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include "vmmdll.h"
#include <iostream>
#include <vector>
#include <algorithm>

#ifdef LINUX

#define REG_NONE                    0
#define REG_SZ                      1
#define REG_EXPAND_SZ               2
#define REG_BINARY                  3
#define REG_DWORD                   4
#define REG_DWORD_LITTLE_ENDIAN     4
#define REG_DWORD_BIG_ENDIAN        5
#define REG_LINK                    6
#define REG_MULTI_SZ                7
#define REG_RESOURCE_LIST           8
#define REG_FULL_RESOURCE_DESCRIPTOR 9
#define REG_RESOURCE_REQUIREMENTS_LIST 10
#define REG_QWORD                   11
#define REG_QWORD_LITTLE_ENDIAN     11

DWORD GetLastError();

#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_ACCESS_DENIED EACCES
#define ERROR_INVALID_HANDLE EBADF
#define ERROR_NOT_ENOUGH_MEMORY ENOMEM
#define ERROR_BAD_FORMAT EINVAL

#pragma pack(push, 1)

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
typedef uintptr_t ULONG_PTR;
typedef intptr_t LONG_PTR;
typedef void* HANDLE;
typedef const char* LPCSTR;
typedef char* LPSTR;

// Directory entry constants
#define IMAGE_DIRECTORY_ENTRY_EXPORT          0
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8
#define IMAGE_DIRECTORY_ENTRY_TLS             9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11
#define IMAGE_DIRECTORY_ENTRY_IAT            12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES     16

// Subsystem constants
#define IMAGE_SUBSYSTEM_UNKNOWN               0
#define IMAGE_SUBSYSTEM_NATIVE                1
#define IMAGE_SUBSYSTEM_WINDOWS_GUI           2
#define IMAGE_SUBSYSTEM_WINDOWS_CUI           3
#define IMAGE_SUBSYSTEM_POSIX_CUI             7
#define IMAGE_SUBSYSTEM_WINDOWS_CE_GUI        9
#define IMAGE_SUBSYSTEM_EFI_APPLICATION      10
#define IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER 11
#define IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER   12
#define IMAGE_SUBSYSTEM_EFI_ROM              13
#define IMAGE_SUBSYSTEM_XBOX                 14
#define IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION 16

// File header characteristics
#define IMAGE_FILE_EXECUTABLE_IMAGE           0x0002
#define IMAGE_FILE_DLL                        0x2000
#define IMAGE_FILE_SYSTEM                     0x1000
#define IMAGE_FILE_32BIT_MACHINE              0x0100

// DLL characteristics
#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT    0x0100
#define IMAGE_DLLCHARACTERISTICS_NO_SEH       0x0400
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000

// PE signature constants
#define IMAGE_DOS_SIGNATURE             0x5A4D      // "MZ"
#define IMAGE_NT_SIGNATURE              0x00004550  // "PE\0\0"
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC   0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC   0x20b


typedef struct _IMAGE_DOS_HEADER {
    WORD   e_magic;                     // 0x00: Magic number
    WORD   e_cblp;                      // 0x02: Bytes on last page of file
    WORD   e_cp;                        // 0x04: Pages in file
    WORD   e_crlc;                      // 0x06: Relocations
    WORD   e_cparhdr;                   // 0x08: Size of header in paragraphs
    WORD   e_minalloc;                  // 0x0A: Minimum extra paragraphs needed
    WORD   e_maxalloc;                  // 0x0C: Maximum extra paragraphs needed
    WORD   e_ss;                        // 0x0E: Initial (relative) SS value
    WORD   e_sp;                        // 0x10: Initial SP value
    WORD   e_csum;                      // 0x12: Checksum
    WORD   e_ip;                        // 0x14: Initial IP value
    WORD   e_cs;                        // 0x16: Initial (relative) CS value
    WORD   e_lfarlc;                    // 0x18: File address of relocation table
    WORD   e_ovno;                      // 0x1A: Overlay number
    WORD   e_res[4];                    // 0x1C: Reserved words
    WORD   e_oemid;                     // 0x24: OEM identifier
    WORD   e_oeminfo;                   // 0x26: OEM information
    WORD   e_res2[10];                  // 0x28: Reserved words
    LONG   e_lfanew;                    // 0x3C: File address of new exe header
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;                    // 0x00: Architecture type
    WORD    NumberOfSections;           // 0x02: Number of sections
    DWORD   TimeDateStamp;              // 0x04: Time/date stamp
    DWORD   PointerToSymbolTable;       // 0x08: Pointer to symbol table
    DWORD   NumberOfSymbols;            // 0x0C: Number of symbols
    WORD    SizeOfOptionalHeader;       // 0x10: Size of optional header
    WORD    Characteristics;            // 0x12: Characteristics flags
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER32 {
    WORD        Magic;                  // 0x00: Magic number
    BYTE        MajorLinkerVersion;     // 0x02: Linker major version
    BYTE        MinorLinkerVersion;     // 0x03: Linker minor version
    DWORD       SizeOfCode;             // 0x04: Size of code section
    DWORD       SizeOfInitializedData;  // 0x08: Size of initialized data
    DWORD       SizeOfUninitializedData;// 0x0C: Size of uninitialized data
    DWORD       AddressOfEntryPoint;    // 0x10: Entry point RVA
    DWORD       BaseOfCode;             // 0x14: Base of code RVA
    DWORD       BaseOfData;             // 0x18: Base of data RVA
    DWORD       ImageBase;              // 0x1C: Preferred load address
    DWORD       SectionAlignment;       // 0x20: Section alignment
    DWORD       FileAlignment;          // 0x24: File alignment
    WORD        MajorOperatingSystemVersion; // 0x28: Major OS version
    WORD        MinorOperatingSystemVersion; // 0x2A: Minor OS version
    WORD        MajorImageVersion;      // 0x2C: Major image version
    WORD        MinorImageVersion;      // 0x2E: Minor image version
    WORD        MajorSubsystemVersion;  // 0x30: Major subsystem version
    WORD        MinorSubsystemVersion;  // 0x32: Minor subsystem version
    DWORD       Win32VersionValue;      // 0x34: Win32 version value
    DWORD       SizeOfImage;            // 0x38: Size of image
    DWORD       SizeOfHeaders;          // 0x3C: Size of headers
    DWORD       CheckSum;               // 0x40: Checksum
    WORD        Subsystem;              // 0x44: Subsystem
    WORD        DllCharacteristics;     // 0x46: DLL characteristics
    DWORD       SizeOfStackReserve;     // 0x48: Size of stack reserve
    DWORD       SizeOfStackCommit;      // 0x4C: Size of stack commit
    DWORD       SizeOfHeapReserve;      // 0x50: Size of heap reserve
    DWORD       SizeOfHeapCommit;       // 0x54: Size of heap commit
    DWORD       LoaderFlags;            // 0x58: Loader flags
    DWORD       NumberOfRvaAndSizes;    // 0x5C: Number of data directories
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; // 0x60: Data directories
} IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    WORD        Magic;                  // 0x00: Magic number
    BYTE        MajorLinkerVersion;     // 0x02: Linker major version
    BYTE        MinorLinkerVersion;     // 0x03: Linker minor version
    DWORD       SizeOfCode;             // 0x04: Size of code section
    DWORD       SizeOfInitializedData;  // 0x08: Size of initialized data
    DWORD       SizeOfUninitializedData;// 0x0C: Size of uninitialized data
    DWORD       AddressOfEntryPoint;    // 0x10: Entry point RVA
    DWORD       BaseOfCode;             // 0x14: Base of code RVA
    ULONGLONG   ImageBase;              // 0x18: Preferred load address
    DWORD       SectionAlignment;       // 0x20: Section alignment
    DWORD       FileAlignment;          // 0x24: File alignment
    WORD        MajorOperatingSystemVersion; // 0x28: Major OS version
    WORD        MinorOperatingSystemVersion; // 0x2A: Minor OS version
    WORD        MajorImageVersion;      // 0x2C: Major image version
    WORD        MinorImageVersion;      // 0x2E: Minor image version
    WORD        MajorSubsystemVersion;  // 0x30: Major subsystem version
    WORD        MinorSubsystemVersion;  // 0x32: Minor subsystem version
    DWORD       Win32VersionValue;      // 0x34: Win32 version value
    DWORD       SizeOfImage;            // 0x38: Size of image
    DWORD       SizeOfHeaders;          // 0x3C: Size of headers
    DWORD       CheckSum;               // 0x40: Checksum
    WORD        Subsystem;              // 0x44: Subsystem
    WORD        DllCharacteristics;     // 0x46: DLL characteristics
    ULONGLONG   SizeOfStackReserve;     // 0x48: Size of stack reserve
    ULONGLONG   SizeOfStackCommit;      // 0x50: Size of stack commit
    ULONGLONG   SizeOfHeapReserve;      // 0x58: Size of heap reserve
    ULONGLONG   SizeOfHeapCommit;       // 0x60: Size of heap commit
    DWORD       LoaderFlags;            // 0x68: Loader flags
    DWORD       NumberOfRvaAndSizes;    // 0x6C: Number of data directories
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; // 0x70: Data directories
} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS32 {
    DWORD                   Signature;      // 0x00: "PE\0\0"
    IMAGE_FILE_HEADER       FileHeader;     // 0x04: File header
    IMAGE_OPTIONAL_HEADER32 OptionalHeader; // 0x18: Optional header
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;

typedef struct _IMAGE_NT_HEADERS64 {
    DWORD                   Signature;      // 0x00: "PE\0\0"
    IMAGE_FILE_HEADER       FileHeader;     // 0x04: File header
    IMAGE_OPTIONAL_HEADER64 OptionalHeader; // 0x18: Optional header
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;

#pragma pack(pop)  // Restore default packing

typedef union _IMAGE_OPTIONAL_HEADER_UNION {
    IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
} IMAGE_OPTIONAL_HEADER_UNION, *PIMAGE_OPTIONAL_HEADER_UNION;

typedef union _IMAGE_NT_HEADERS_UNION {
    DWORD Signature;
    struct {
        DWORD Signature;
        IMAGE_FILE_HEADER FileHeader;
        // OptionalHeader follows...
    } Common;
    IMAGE_NT_HEADERS32 Headers32;
    IMAGE_NT_HEADERS64 Headers64;
} IMAGE_NT_HEADERS_UNION, *PIMAGE_NT_HEADERS_UNION;

#define IMAGE_FIRST_SECTION32(ntheader) ((PIMAGE_SECTION_HEADER) \
    ((ULONG_PTR)(ntheader) + \
    sizeof(DWORD) + \
    sizeof(IMAGE_FILE_HEADER) + \
    ((PIMAGE_NT_HEADERS32)(ntheader))->FileHeader.SizeOfOptionalHeader))  // CHANGED: FileHeader.SizeOfOptionalHeader

#define IMAGE_FIRST_SECTION64(ntheader) ((PIMAGE_SECTION_HEADER) \
    ((ULONG_PTR)(ntheader) + \
    sizeof(DWORD) + \
    sizeof(IMAGE_FILE_HEADER) + \
    ((PIMAGE_NT_HEADERS64)(ntheader))->FileHeader.SizeOfOptionalHeader))  // CHANGED: FileHeader.SizeOfOptionalHeader

#define IMAGE_FIRST_SECTION_UNION(pNtUnion) \
    ((PIMAGE_SECTION_HEADER) \
        ((ULONG_PTR)(pNtUnion) + \
        offsetof(IMAGE_NT_HEADERS_UNION, Headers32) + \
        sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + \
        ((pNtUnion)->Common.FileHeader.SizeOfOptionalHeader)))
#endif

enum class e_registry_type
{
	none = REG_NONE,
	sz = REG_SZ,
	expand_sz = REG_EXPAND_SZ,
	binary = REG_BINARY,
	dword = REG_DWORD,
	dword_little_endian = REG_DWORD_LITTLE_ENDIAN,
	dword_big_endian = REG_DWORD_BIG_ENDIAN,
	link = REG_LINK,
	multi_sz = REG_MULTI_SZ,
	resource_list = REG_RESOURCE_LIST,
	full_resource_descriptor = REG_FULL_RESOURCE_DESCRIPTOR,
	resource_requirements_list = REG_RESOURCE_REQUIREMENTS_LIST,
	qword = REG_QWORD,
	qword_little_endian = REG_QWORD_LITTLE_ENDIAN
};

extern uint32_t pid;
extern uint64_t gafAsyncKeyStateExport;
extern uint8_t state_bitmap[];
extern uint8_t previous_state_bitmap[];
extern uint64_t win32kbase;

extern int win_logon_pid;

//e_registry_type registry;

extern std::chrono::time_point<std::chrono::system_clock> start;

struct Info
{
    uint32_t index;
    uint32_t process_id;
    uint64_t dtb;
    uint64_t kernelAddr;
    char name[256];
};

extern VMM_HANDLE hVMM;
extern std::string process_name;
extern std::string DLL_Name;
extern uint32_t process_id;
extern HANDLE process_handle;
extern ULONG64 process_base_address;
extern ULONG64 DLL_base_address;
extern DWORD process_size;
extern DWORD DLL_size;
extern std::string driver_name;
extern ULONG64 driver_base_address;
extern DWORD driver_size;

bool Initialize();

bool Initialize_with_exe(const std::string process_name);

bool InitializeDLL(const std::string process_name, const std::string DLL_Name);

bool InitializeDriver(const std::string driver_name);

bool DumpDriver();

VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo);

bool vmmdll_read(uint64_t address, void* buffer, size_t size);

bool vmmdll_write(uint64_t address, void* buffer, size_t size);

template <typename T>
T dma_read(void* address)
{
	T buffer{ };
	memset(&buffer, 0, sizeof(T));
	vmmdll_read(reinterpret_cast<uint64_t>(address), reinterpret_cast<void*>(&buffer), sizeof(T));

	return buffer;
}

template <typename T>
T dma_read(uint64_t address)
{
	return dma_read<T>(reinterpret_cast<void*>(address));
}

template <typename T>
T dma_read(uint64_t address, uint32_t pid)
{
	return dma_read<T>(address);
}

template <typename T>
bool dma_write(void* address, T value)
{
	return vmmdll_write(address, &value, sizeof(T));
}

template <typename T>
bool dma_write(uintptr_t address, T value)
{
	return vmmdll_write(address, &value, sizeof(T));
}


bool read_buffer(uintptr_t address, void* buffer, size_t size);

VMMDLL_SCATTER_HANDLE CreateScatterHandle();

void CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle);

void AddScatterRead(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size);

void ExecuteScatterRead(VMMDLL_SCATTER_HANDLE handle);

bool vmmdll_write(uint64_t address, void* buffer, size_t size);

template<class T> T dma_write(uintptr_t address);

uint32_t get_process_id(const std::string process_name);

bool get_process_base_address(const std::string process_name, const uint32_t& process_id);

bool GetDLLModuleBase(const uint32_t& process_id, const std::string DLL_Name);

bool DumpExe();

bool DumpDLL();

uintptr_t PatternScan1(void* module, const char* signature, const char* sectionName = nullptr, int skip = 0);

uintptr_t PatternScan(const char* signature);

bool DumpExe();

bool DumpDLL();

bool InitKeyboard();

void UpdateKeys();

bool IsKeyDown(uint32_t virtual_key_code);

std::vector<int> GetPidListFromName(std::string name);

const char* LPWSTR_TO_CC(LPWSTR in);

LPSTR CC_TO_LPSTR(const char* in);

std::string QueryValue(const char* path, e_registry_type type);
