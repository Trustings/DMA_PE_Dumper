# DMA_PE_Dumper

DMA_PE_Dumper is a DMA capable portable executable dumper that can work over a PCIe FPGA device or if utilizing a Virtual Machine, shared memory or hugepages. It is currently cross compatible with Linux and Windows.

Currently supports the dumping of both x86 and x64 system drivers, native executables, native executables and their dynamic link libraries. 

<img width="1920" height="1080" alt="Screenshot From 2026-08-10 02-50-00" src="https://github.com/user-attachments/assets/1eb24488-a800-491c-80e8-3a43707518d0" />

# Building:

# Linux

While in the root project directory...
```
mkdir build
cd build
cmake ../
make
```
Please note: To have this run successfully on your Linux machine you must configure your backend as such. https://github.com/ufrisk/LeechCore/wiki/Device_QEMU


# Examples:

Once built, cd into the working build directory, input the name of either a system driver, a target exe, or a target exe with an associative dll.

EXAMPLE 1 -> ./DMA_PE_Dumper YourTarget.sys

EXAMPLE 2 -> ./DMA_PE_Dumper YourTarget.exe 

EXAMPLE 3 -> ./DMA_PE_Dumper YourTarget.exe YourTarget.dll

