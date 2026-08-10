# DMA_PE_Dumper

DMA_PE_Dumper is a DMA capable portable executable dumper that can work over a PCIe FPGA device or on Virtual Machine hugepages. It is currently cross compatible with Linux and Windows.

Currently supports the dumping of both x86 and x64 system drivers, native executables, native executables and their dynamic link libraries. 

<img width="1920" height="1080" alt="Screenshot From 2026-08-10 02-50-00" src="https://github.com/user-attachments/assets/1eb24488-a800-491c-80e8-3a43707518d0" />

<img width="1920" height="1080" alt="Screenshot From 2026-08-10 02-59-46" src="https://github.com/user-attachments/assets/06d5eb04-9074-4d6c-bfa9-9c711de7d1da" />

<img width="1920" height="1080" alt="Screenshot From 2026-08-10 02-57-42" src="https://github.com/user-attachments/assets/eb44953c-a37f-4baf-b5e3-a0469d4c33c1" />

# Building:

# Linux

While in the root project directory...
```
mkdir build
cd build
cmake ../
make
```



# Examples:

Once built, cd into the working build directory, input the name of either a system driver, a target exe, or a target exe with an associative dll.

EXAMPLE 1 -> ./DMA_PE_Dumper YourTarget.sys

EXAMPLE 2 -> ./DMA_PE_Dumper YourTarget.exe 

EXAMPLE 3 -> ./DMA_PE_Dumper YourTarget.exe YourTarget.dll

