# vhal-core — Android 16 VHAL Linux/QNX Port

## Project Goal
Port Android 16 Vehicle HAL (VHAL) to run on Linux (Raspberry Pi OS) and QNX,
replacing Android's Binder/AIDL IPC transport with gRPC (current) and SOME/IP (planned).
ClusterUI and other apps derive vehicle property data from VHAL running on non-Android OS.
Any app or service must be able to read or inject vehicle properties via the IPC transport.

## Planned Package Split
The codebase is being restructured into three independently distributable Conan packages:

1. **vhal-types** — transport-agnostic domain layer
   - AIDL-generated C++ headers (VehicleProperty.h, VehicleGear.h, etc.)
   - IVehicleHardware abstract interface
   - VehicleHalUtils (VehiclePropertyStore, RecurrentTimer, etc.)
   - Zero transport dependency — survives any IPC change (gRPC → SOME/IP)

2. **vhal-ipc-grpc** — gRPC transport layer (current, replaceable)
   - Protobuf message definitions and generated stubs
   - VehicleServer.proto (service contract)
   - GRPCVehicleProxyServer (server-side adapter)
   - Intended to be swapped for vhal-ipc-someip when SOME/IP is adopted

3. **vhal-server** — runnable server process
   - FakeVehicleHardware simulation backend
   - DefaultProperties.json config loading
   - main() entry point
   - Depends on vhal-types + one vhal-ipc-* package

Clients (ClusterUI etc.) depend only on vhal-types + vhal-ipc-grpc (for the generated
client stub VehicleServer::Stub). They have no compile-time dependency on vhal-server.

## Architecture
- Current transport: gRPC replaces Binder (GRPCVehicleProxyServer is the entry point)
- Planned transport: SOME/IP (vsomeip) — vhal-ipc-grpc replaced by vhal-ipc-someip
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
- GRPCVehicleProxyServer.h must be included before FakeVehicleHardware.h in any TU
  that includes both — grpc networking headers define TRY_AGAIN (netdb.h macro) which
  conflicts with the proto-generated StatusCode enum if included first

## Build Instructions
conan install . --output-folder=build/Release --build=missing
cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j$(nproc)

## Current Status
Core server builds and runs. gRPC connection tracking (new peer / disconnect) implemented.
Next: restructure CMakeLists.txt and add conanfile.py exports for the three-package split.
