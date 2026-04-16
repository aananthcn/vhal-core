# vhal-core — Android 16 VHAL Linux/QNX Port

## Project Goal
Port Android 16 Vehicle HAL (VHAL) to run on Linux (Raspberry Pi OS) and QNX,
replacing Android's Binder/AIDL IPC transport with gRPC (current) and SOME/IP (planned).
ClusterUI and other apps derive vehicle property data from VHAL running on non-Android OS.
Any app or service must be able to read or inject vehicle properties via the IPC transport.

## Package Structure
Four Linux packages (CMake + Conan) plus one Android-only package (Soong):

1. **vhal-types** — transport-agnostic domain layer
   - AIDL-generated C++ headers (VehicleProperty.h, VehicleGear.h, etc.)
   - IVehicleHardware abstract interface
   - VehicleHalUtils (VehiclePropertyStore, RecurrentTimer, etc.)
   - Zero transport dependency — survives any IPC change (gRPC → SOME/IP)

2. **vhal-ipc-grpc** — gRPC transport layer (current, replaceable)
   - Protobuf message definitions and generated stubs
   - VehicleServer.proto (service contract)
   - GRPCVehicleProxyServer (server-side adapter) + GRPCVehicleHardware (client adapter)
   - Intended to be swapped for vhal-ipc-someip when SOME/IP is adopted

3. **vhal-server** — runnable server process (Linux only)
   - FakeVehicleHardware simulation backend
   - DefaultProperties.json config loading
   - main() entry point
   - Depends on vhal-types + one vhal-ipc-* package

4. **vhal-gateway** — property forwarding daemon (Linux only)
   - Subscribes to local vhal-core via GRPCVehicleHardware (StartPropertyValuesStream)
   - On property change, forwards matching values to configured remote nodes via SetValues
   - Config: packages/vhal-gateway/etc/vhal/gateway-configs.json (source truth)
   - Runtime config root: /opt/car-ui/etc/vhal (override via -DGATEWAY_CONFIG_ROOT=...)
   - One worker thread spawned per remote node; callback thread never blocked
   - Config JSON: { version, gatewayNodes: [ { ipaddr, messages: [ { msgId, properties: [ { id, desc } ] } ] } ] }
   - Property id is a hex string (e.g. "0x11400400"); desc is informational only
   - Each message group is forwarded in its own SetValues call; groups with no changed props are skipped

5. **vhal-bridge** — Android VHAL service (Android / Soong only)
   - Built by Soong when this repo is placed at vendor/brcm/vhal-core/ in an AOSP tree
   - NOT built by CMake — packages/vhal-bridge/ has no CMakeLists.txt
   - Uses GRPCVehicleHardware (client side of vhal-ipc-grpc) as the IVehicleHardware backend
   - Wraps it in DefaultVehicleHal and registers as android.hardware.automotive.vehicle IVehicle/default
   - Replaces android.hardware.automotive.vehicle@V4-default-service on the target device
   - Server address: 192.168.10.10:50051 (default); override via vendor.vhal.grpc.server property
   - Files: packages/vhal-bridge/main.cpp, Android.bp, vhal-grpc-service.rc, vhal-grpc-service.xml

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
- Linux build: CMake + Conan; Android build: Soong (Android.bp) — one repo, two pipelines
- Android.bp files exist only under vhal-ipc-grpc/ and vhal-bridge/ for the Soong pipeline
- vhal-bridge/ has no CMakeLists.txt (Android-only); vhal-gateway/ has no Android.bp (Linux-only)
- Binder/libbinder_ndk is stubbed out on Linux — not used on gRPC path
- LargeParcelableBase is stubbed as no-op — gRPC handles large messages natively
- GRPCVehicleProxyServer.h must be included before FakeVehicleHardware.h in any TU
  that includes both — grpc networking headers define TRY_AGAIN (netdb.h macro) which
  conflicts with the proto-generated StatusCode enum if included first

## Build Instructions

### Linux (CMake + Conan)
```bash
conan install . --output-folder=build/Release --build=missing
cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j$(nproc)
```
Produces: vhal-server binary, vhal-gateway binary.

### Android (Soong — from AOSP root)
```bash
# Place (or clone) this repo at vendor/brcm/vhal-core/ in your AOSP tree, then:
mmm vendor/brcm/vhal-core
```
Produces: `android.hardware.automotive.vehicle@V4-grpc-service` (vendor/bin/hw/).

Add to device.mk to activate:
```makefile
PRODUCT_PACKAGES -= android.hardware.automotive.vehicle@V4-default-service
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V4-grpc-service
```

## Current Status
Five-package split: vhal-types, vhal-ipc-grpc, vhal-server, vhal-gateway (all Linux),
plus vhal-bridge (Android only). Core server builds and runs. gRPC connection tracking
(new peer / disconnect) implemented. vhal-gateway reads gateway-configs.json and forwards
on-change property values to configured remote nodes over gRPC SetValues.
vhal-bridge packages the gRPC client as an Android VHAL service, allowing Android clients
(rvc_service, CarService) to receive property changes injected into vhal-core from any
external source (Python test scripts, other nodes, CAN adapters).
