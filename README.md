# DMA_PE_Dumper

DMA_PE_Dumper is a DMA capable portable executable dumper that can work over a PCIe FPGA device or on Virtual Machine hugepages. It is currently cross compatible with Linux and Windows.

Currently supports the dumping of both x86 and x64 system drivers, native executables, native executables and their dynamic link libraries. 

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

