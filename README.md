# DMA_PE_Dumper

DMA Portable Executable Dumper over a PCIe FPGA device with full support against CR3 shuffling.

You will need to add the following DLLs to the x64 project output folder, -> leechcore.dll, vmm.dll, and FTD3XX.dll.

Once built, CD into the working directory of the output folder, type the compiled EXE name into the command prompt and your target EXE.

EXAMPLE -> DMA_PE_Dumper.exe YourTarget.exe

**VMMDLL_MemRead failed** IS NORMAL, do not close out. It just means a DTB match was not found at that physical address. The matching can sometimes take up to a minute due to the method of obtaining the DTB, which involves bruteforcing the alignment of physical memory pages for potential candidates. If I find a better method for updation I will commit it to the repo.
