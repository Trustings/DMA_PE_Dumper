# DMA_PE_Dumper

DMA Portable Executable Dumper over a PCIe FPGA device with full support against CR3 shuffling.

You will need to add the following DLLs to the x64 project output folder, -> leechcore.dll, vmm.dll, and FTD3XX.dll.

Once built, CD into the active directory of the output folder, type the compiled EXE name into the command prompt and your target EXE. 

EXAMPLE -> DMA_PE_Dumper.exe YourTarget.exe
