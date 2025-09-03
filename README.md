DMA_PE_Dumper Guide:
1. **Get Ready**:
   - Have game running on desktop/pc1, these next steps will be done on desktop/pc2.
   - Put these files in the same folder as `DMA_PE_Dumper.exe`: `leechcore.dll`, `vmm.dll`, and `FTD3XX.dll`.
2. **Open Dumper Exe**:
   - Enter Target Executable & The Dll is optional. then press start dump. 
3. **Wait**:
   - If you see `VMMDLL_MemRead failed`, it’s normal. It might take up to a minute—just let it finish.
   - Note: This tool uses a PCIe FPGA device and supports CR3 shuffling. The `VMMDLL_MemRead failed` error means a DTB match wasn’t found, and the tool will bruteforce memory pages for up to a minute to find it.

![PEdumper](https://github.com/user-attachments/assets/f6488bf3-64f2-4fe8-b30d-9dc8af2b46a9)
