# vhal-core — Android 16 VHAL Linux/QNX Port

## Project Goal
Port Android 16 Vehicle HAL (VHAL) to run on Linux (Raspberry Pi OS) and QNX,
replacing Android's Binder/AIDL IPC transport with gRPC (current) and SOME/IP (planned).
ClusterUI and other apps derive vehicle property data from VHAL running on non-Android OS.
Any app or service must be able to read or inject vehicle properties via the IPC transport.

## Domain Isolation Principle
Each physical node (Linux IC, Android HU, future QNX node) runs its own vhal-core server.
- **Freedom from interference**: a node's VHAL stack operates independently; it retains
  last-known property values even if all other nodes are unreachable.
- **Controlled sharing**: the only cross-domain channel is vhal-gateway, running on the
  Linux IC, which pushes a filtered, configured property set to each remote node's
  vhal-core server via gRPC SetValues. Remote nodes are invisible to local VHAL clients.

## Package Structure
Five packages across two build pipelines:

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

3. **vhal-server** — runnable gRPC server process (Linux AND Android)
   - FakeVehicleHardware simulation backend
   - DefaultProperties.json config loading
   - VehicleService.cpp entry point (uses GRPCVehicleProxyServer)
   - Linux build: CMakeLists.txt → binary `vhal-core`
   - Android build: Android.bp → binary `vhal-core-server` (installs to /vendor/bin/)
   - On Android, listens on 0.0.0.0:50051 — accepts gateway pushes from Linux IC and
     serves vhal-bridge locally via 127.0.0.1:50051

4. **vhal-gateway** — property forwarding daemon (Linux IC only)
   - Subscribes to local vhal-core via GRPCVehicleHardware (StartPropertyValuesStream)
   - On property change, forwards matching values to configured remote nodes via SetValues
   - Config: packages/vhal-gateway/etc/vhal/gateway-configs.json (source truth)
   - Runtime config root: /opt/car-ui/etc/vhal (override via -DGATEWAY_CONFIG_ROOT=...)
   - One worker thread spawned per remote node; callback thread never blocked
   - Config JSON: { version, gatewayNodes: [ { ipaddr, messages: [ { msgId, properties: [ { id, desc } ] } ] } ] }
   - Property id is a hex string (e.g. "0x11400400"); desc is informational only
   - Each message group is forwarded in its own SetValues call; groups with no changed props are skipped
   - In production: ipaddr points to each remote node's vhal-core server (e.g. Android HU at 192.168.10.20:50051)

5. **vhal-bridge** — Android AIDL VHAL frontend (Android / Soong only)
   - Built by Soong when this repo is placed at vendor/brcm/vhal-core/ in an AOSP tree
   - NOT built by CMake — packages/vhal-bridge/ has no CMakeLists.txt
   - Uses GRPCVehicleHardware connecting to the LOCAL vhal-core-server (127.0.0.1:50051)
   - Never connects to the Linux IC — domain isolation enforced here
   - Wraps it in DefaultVehicleHal and registers as android.hardware.automotive.vehicle IVehicle/default
   - Replaces android.hardware.automotive.vehicle@V4-default-service on the target device
   - Server address: 127.0.0.1:50051 (default); override via vendor.vhal.grpc.server property
   - Files: packages/vhal-bridge/main.cpp, Android.bp, vhal-grpc-service.rc, vhal-grpc-service.xml

Clients (ClusterUI etc.) depend only on vhal-types + vhal-ipc-grpc (for the generated
client stub VehicleServer::Stub). They have no compile-time dependency on vhal-server.

## Architecture
- Current transport: gRPC replaces Binder (GRPCVehicleProxyServer is the entry point)
- Planned transport: SOME/IP (vsomeip) — vhal-ipc-grpc replaced by vhal-ipc-someip
- Build system: CMake + Conan (Linux), Soong / Android.bp (Android)
- Source: Extracted from AOSP hardware/interfaces/automotive/vehicle/

## Key Decisions
- aidl/impl/4/ contains the implementation (Android 16 = V4)
- aidl_property/ contains property definitions (VehicleProperty.aidl etc.)
- stubs/ contains Linux replacements for Android-specific headers
- scripts/generate_aidl_headers.py converts .aidl files to C++ headers
- Linux build: CMake + Conan; Android build: Soong (Android.bp) — one repo, two pipelines
- vhal-server has BOTH CMakeLists.txt (Linux) AND Android.bp (Android) — runs on both nodes
- vhal-bridge/ has no CMakeLists.txt (Android-only); vhal-gateway/ has no Android.bp (Linux IC-only)
- Only TWO Android.bp files exist in the entire vhal-core repo:
    packages/vhal-server/Android.bp  → builds vhal-core-server (unique name, not in AOSP)
    packages/vhal-bridge/Android.bp  → builds V4-grpc-service (unique name, not in AOSP)
  All other Android.bp files (in vhal-types/, vhal-ipc-grpc/, vhal-server/aidl/) were AOSP
  verbatim copies. They are intentionally deleted — AOSP already provides those modules at
  hardware/interfaces/automotive/vehicle/. Keeping them in vendor/ causes "module already
  defined" duplicate errors in Soong. The two surviving Android.bp files reference AOSP
  modules (FakeVehicleHardware, DefaultVehicleHal, etc.) by name, which Soong resolves
  from the platform source.
- vhal-bridge default address is 127.0.0.1:50051 — connects only to the local vhal-core-server
- vhal-gateway is the sole cross-domain channel; its config lists the remote node IPs
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
Produces: vhal-core (gRPC server binary), vhal-gateway binary.

### Android (Soong — from AOSP root)
```bash
# Place (or clone) this repo at vendor/brcm/vhal-core/ in your AOSP tree, then:
mmm vendor/brcm/vhal-core
```
Produces:
- `vendor/bin/vhal-core-server` — local gRPC server (receives gateway pushes from Linux IC)
- `vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service` — AIDL bridge

Add to device.mk to activate:
```makefile
PRODUCT_PACKAGES -= android.hardware.automotive.vehicle@V4-default-service
PRODUCT_PACKAGES += vhal-core-server
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V4-grpc-service
```

## Current Status
Five-package split: vhal-types, vhal-ipc-grpc, vhal-server, vhal-gateway, vhal-bridge.
vhal-server now builds on both Linux (CMake) and Android (Soong) — each node runs its own
gRPC server. vhal-gateway is the sole controlled cross-domain channel, forwarding selected
properties from the Linux IC to the Android HU's local vhal-core-server.
vhal-bridge connects only to the local vhal-core-server (127.0.0.1:50051), enforcing
domain isolation — Android VHAL clients are fully isolated from the Linux IC domain.
