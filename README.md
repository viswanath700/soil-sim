# SoilSim

This repository is the starting scaffold for the SoilSim excavation simulation project.

## Prerequisites
- Windows 11
- Visual Studio 2022 Community or later with the C++ workload
- CMake 3.24+
- Ninja build tool
- Python 3 (for future plotting and scenario tooling)

## Configure, build, and test
From a Developer Command Prompt for Visual Studio 2022:

```bat
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

For the optimized build:

```bat
cmake --preset release
cmake --build --preset release
ctest --preset release
```
