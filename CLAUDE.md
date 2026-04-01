# vhal-core — Android 16 VHAL Linux/QNX Port

## Project Goal
Port Android 16 Vehicle HAL (VHAL) to run on Linux (Raspberry Pi OS) and QNX,
replacing Android's Binder/AIDL IPC transport with gRPC.
ClusterUI will derive vehicle property data from VHAL running on non-Android OS.

## Architecture
- Transport: gRPC replaces Binder (GRPCVehicleProxyServer is the entry point)
- Build system: CMake + Conan
- Source: Extracted from AOSP hardware/interfaces/automotive/vehicle/

## Key Decisions
- aidl/impl/4/ contains the implementation (Android 16 = V4)
- aidl_property/ contains property definitions (VehicleProperty.aidl etc.)
- stubs/ contains Linux replacements for Android-specific headers
- scripts/generate_aidl_headers.py converts .aidl files to C++ headers
- All Android.bp files are replaced by CMakeLists.txt
- Binder/libbinder_ndk is stubbed out — not used on gRPC path
- LargeParcelableBase is stubbed as no-op — gRPC handles large messages natively

## Build Instructions
conan install . --output-folder=build/Release --build=missing
cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake
cmake --build build/Release -j$(nproc)

## Current Status
Actively fixing compilation errors in stub headers under stubs/
Working through layers bottom-up: VehicleHalUtils -> fake_impl -> grpc -> vhal
