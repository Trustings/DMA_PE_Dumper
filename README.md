# DMA_PE_Dumper

DMA Portable Executable Dumper over a PCIe FPGA device with full support against CR3 shuffling.

You will need to add the following DLLs to the x64 project output folder, -> leechcore.dll, vmm.dll, and FTD3XX.dll.

Once built, CD into the working directory of the output folder, type the compiled EXE name into the command prompt, your target EXE, and optionally a DLL of the target process.

EXAMPLE 1 -> DMA_PE_Dumper.exe YourTarget.exe 

EXAMPLE 2 -> DMA_PE_Dumper.exe YourTarget.exe YourTarget.dll

**VMMDLL_MemRead failed** IS NORMAL, do not close out. It just means a DTB match was not found at that physical address. The matching can sometimes take up to a minute due to the method of obtaining the DTB, which involves bruteforcing the alignment of physical memory pages for potential candidates. If I find a better method for updation I will commit it to the repo.

![PEdumper](https://github.com/user-attachments/assets/f6488bf3-64f2-4fe8-b30d-9dc8af2b46a9)
