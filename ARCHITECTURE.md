# vhal-core — Architecture

## Overview

vhal-core is a Linux/QNX port of the Android 16 Vehicle HAL (VHAL). It replaces Android's
Binder/AIDL IPC transport with gRPC, allowing any non-Android client (e.g. ClusterUI running
on Linux or QNX) to read and write vehicle properties over a standard TCP or Unix Domain Socket.

The codebase is structured for eventual distribution as three independent Conan packages,
with a clear separation between transport-agnostic domain types and the replaceable IPC layer.
This allows the gRPC transport to be swapped for SOME/IP (or any other IPC) without touching
the domain model or the clients that depend on it.

Source is selectively extracted from AOSP at:
```
hardware/interfaces/automotive/vehicle/
```

The implementation version is **V4**, corresponding to Android 16.

---

## Domain Isolation Principle

Each physical node runs its own vhal-core server. Android VHAL clients never communicate
directly across domain boundaries.

- **Freedom from interference**: if the Linux IC goes down, the Android HU retains last-known
  property values and its VHAL stack remains operational.
- **Controlled sharing**: the only cross-domain channel is vhal-gateway, which runs on the Linux
  IC and pushes a filtered, configured set of properties into the Android node's local vhal-core
  server via gRPC SetValues. Android clients are unaware of the Linux IC's existence.

---

## Package Split

```
┌───────────────────────────────────────────────────────────────────────────┐
│  vhal-proto          (proto text files — platform-independent)            │
│                                                                           │
│  VehicleServer.proto (gRPC service definition)                            │
│  android/hardware/automotive/vehicle/*.proto  (19 message-type protos)    │
│                                                                           │
│  Conan package_type = "header-library".  No settings — same binary for   │
│  all architectures.  Consumed by any external project (Velan, ClusterUI) │
│  that needs to run protoc without a hardcoded path into vhal-core.        │
└───────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  vhal-types          (transport-agnostic, permanent)            │
│                                                                 │
│  AIDL-generated headers:  VehicleProperty.h, VehicleGear.h,    │
│  VehiclePropValue.h, StatusCode.h, ... (117 property types)     │
│  IVehicleHardware.h  (abstract backend interface)               │
│  VehicleHalUtils:    VehiclePropertyStore, RecurrentTimer, etc. │
│                                                                 │
│  Zero transport dependency.  Survives gRPC → SOME/IP migration. │
└───────────────────────────┬─────────────────────────────────────┘
                            │ depends on
          ┌─────────────────┴──────────────────┐
          │                                    │
┌─────────▼──────────────────┐    ┌────────────▼──────────────────┐
│  vhal-ipc-grpc             │    │  vhal-ipc-someip  (future)    │
│  (current transport)       │    │  vsomeip stubs                │
│                            │    │  SomeIPVehicleProxyServer     │
│  VehicleServer.proto       │    │  SomeIPVehicleHardware        │
│  Generated .pb.h/.grpc.pb.h│    │                               │
│  libVehicleHalProtos       │    │  Drop-in replacement for      │
│  libVehicleServerProtoStub │    │  vhal-ipc-grpc                │
│  GRPCVehicleProxyServer    │    └───────────────────────────────┘
│  GRPCVehicleHardware       │
└──────┬──────────┬──────────┘
       │          │ depends on
       │   ┌──────▼──────────────┐    ┌──────────────────────────────┐
       │   │  vhal-server        │    │  vhal-gateway                │
       │   │  (Linux + Android)  │    │  (Linux only)                │
       │   │                     │    │                              │
       │   │  FakeVehicleHardware│    │  GatewayConfig (JSON loader) │
       │   │  DefaultProperties  │    │  VhalGateway (orchestrator)  │
       │   │  VehicleService.cpp │    │  NodeForwarder × N           │
       │   │  CMakeLists.txt     │    │  (one thread per remote node)│
       │   │  Android.bp (new)   │    └──────────────────────────────┘
       │   └─────────────────────┘
       │ depends on (Android only)
       │   ┌──────────────────────────────────────────────────────┐
       └──►│  vhal-bridge                                         │
           │  (Android VHAL service — AIDL frontend)              │
           │                                                      │
           │  GRPCVehicleHardware → 127.0.0.1:50051 (local)      │
           │  DefaultVehicleHal   (AIDL VHAL framework)           │
           │  main.cpp            (registers IVehicle/default)    │
           │  Android.bp / .rc / .xml                             │
           │                                                      │
           │  Built by Soong only.  No CMakeLists.txt.            │
           └──────────────────────────────────────────────────────┘
```

**Client dependency (ClusterUI, etc.):**
```
ClusterUI → vhal-types + vhal-ipc-grpc (VehicleServer::Stub only)
```
ClusterUI has no compile-time dependency on vhal-server or GRPCVehicleProxyServer.
The server-side class and the client-side Stub both come from the same generated
`VehicleServer.grpc.pb.h`, but only the Stub is used by clients.

**Android client dependency (vhal-bridge):**
```
vhal-bridge → vhal-ipc-grpc (GRPCVehicleHardware only) + DefaultVehicleHal
```
vhal-bridge has no dependency on GRPCVehicleProxyServer, and never connects to the
Linux IC. It connects only to the local vhal-core server on the same Android node.

---

## Build Pipelines

The repo is pulled into **two independent build pipelines** from the same source tree.
Each build system sees only what it knows about:

```
vhal-core/
├── CMakeLists.txt          ← Linux pipeline entry point (CMake + Conan)
├── packages/
│   ├── vhal-types/
│   │   └── CMakeLists.txt  ← Linux builds this — NO Android.bp (AOSP provides these modules)
│   ├── vhal-ipc-grpc/
│   │   └── CMakeLists.txt  ← Linux builds this — NO Android.bp (AOSP provides these modules)
│   ├── vhal-server/
│   │   ├── CMakeLists.txt  ← Linux builds this (vhal-core server binary)
│   │   └── Android.bp      ← Android builds this (vhal-core-server binary) ← ONLY NEW MODULE
│   ├── vhal-gateway/
│   │   └── CMakeLists.txt  ← Linux builds this — NO Android.bp → Soong ignores
│   └── vhal-bridge/
│       └── Android.bp      ← Android builds this — NO CMakeLists.txt → CMake ignores ← ONLY NEW MODULE
```

**Why only two Android.bp files?**
All sub-package Android.bp files inside vhal-types/, vhal-ipc-grpc/, and vhal-server/aidl/ were
verbatim copies of AOSP modules from `hardware/interfaces/automotive/vehicle/`. When vhal-core is
placed at `vendor/brcm/vhal-core/` in an AOSP tree, Soong finds both the platform copy and the
vendor copy of each module and fails with "module already defined". These duplicate files have been
intentionally deleted. The two surviving Android.bp files define **new** module names that do not
exist in the platform:
- `vhal-core-server` — the local gRPC server binary for the Android HU
- `android.hardware.automotive.vehicle@V4-grpc-service` — the AIDL bridge binary

Both reference existing AOSP modules (FakeVehicleHardware, DefaultVehicleHal, VehicleHalUtils, etc.)
by name; Soong resolves them from `hardware/interfaces/automotive/vehicle/`.

**Linux pipeline** — builds the server and gateway binaries:
```bash
# Repo location: ~/labs/ui/ic-rpi/src/vhal-core/  (or any path)
conan install . --output-folder=build/Release --build=missing
cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release -j$(nproc)
# Produces: vhal-core (server binary), vhal-gateway binary
```

**Android pipeline** — builds the local gRPC server and the AIDL bridge:
```bash
# Repo location: <AOSP_ROOT>/vendor/brcm/vhal-core/
mmm vendor/brcm/vhal-core
# Produces:
#   vendor/bin/vhal-core-server            (local gRPC server)
#   vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service  (AIDL bridge)
```

**device.mk change** to activate both on Android:
```makefile
# Replace the default fake VHAL with the gRPC-backed bridge
PRODUCT_PACKAGES -= android.hardware.automotive.vehicle@V4-default-service
PRODUCT_PACKAGES += vhal-core-server
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V4-grpc-service
```

---

## IPC Transport Swap Path (gRPC → SOME/IP)

When SOME/IP is adopted:

| Layer | Change required |
| :--- | :--- |
| vhal-types | None — property IDs, value types, IVehicleHardware unchanged |
| vhal-ipc-grpc | Replaced entirely by vhal-ipc-someip |
| vhal-server | Relink against vhal-ipc-someip; no domain logic changes |
| vhal-gateway | Swap GRPCVehicleHardware for SomeIPVehicleHardware; NodeForwarder uses SOME/IP SetValues |
| vhal-bridge | Swap GRPCVehicleHardware for SomeIPVehicleHardware in main.cpp; Android.bp deps updated |
| ClusterUI | Swap VhalGrpcClient for VhalSomeIPClient; property access code unchanged |

---

## Runtime Block Diagram

```
  Linux IC domain (Node A: 192.168.10.10)         Android HU domain (Node B: 192.168.10.20)
  ══════════════════════════════════════════       ══════════════════════════════════════════

  ┌─────────────────────────────────┐              ┌──────────────────────────────────────┐
  │  ClusterUI / any Linux client   │              │  rvc_service / CarService / etc.     │
  │  VhalGrpcClient                 │              │  (AIDL VHAL client)                  │
  │    └── VehicleServer::Stub      │              └──────────────┬───────────────────────┘
  └──────────────┬──────────────────┘                             │ AIDL binder
                 │ gRPC                                           ▼
                 ▼                              ┌──────────────────────────────────────────┐
  ┌──────────────────────────────────┐          │  vhal-bridge                             │
  │  vhal-core server (primary)      │          │  DefaultVehicleHal                       │
  │  GRPCVehicleProxyServer          │          │    └── GRPCVehicleHardware               │
  │    └── FakeVehicleHardware       │          │         (127.0.0.1:50051)               │
  │          └── PropertyStore       │          └──────────────┬───────────────────────────┘
  │          └── GeneratorHub        │                         │ gRPC (loopback)
  └──────────────▲───────────────────┘                         ▼
                 │ StartPropertyValuesStream     ┌──────────────────────────────────────────┐
  ┌──────────────┴───────────────────┐           │  vhal-core server (local)                │
  │  vhal-gateway                    │─gRPC─────►│  GRPCVehicleProxyServer                  │
  │  (controlled cross-domain        │  SetValues│    └── FakeVehicleHardware               │
  │   boundary — ONLY path in)       │           │          └── PropertyStore               │
  │  NodeForwarder × N               │           └──────────────────────────────────────────┘
  │  (filtered property set)         │
  └──────────────────────────────────┘
```

**Data flow for gear change (Python test → Android rvc_service):**
```
python vhal_test_client.py --gear reverse --server 192.168.10.10
  → Linux vhal-core gRPC server (SetValues GEAR_SELECTION=8)
    → FakeVehicleHardware (property updated, change event fired)
      → GRPCVehicleProxyServer (StartPropertyValuesStream push to vhal-gateway)
        → vhal-gateway NodeForwarder (GEAR_SELECTION listed in gateway-configs.json)
          → Android vhal-core server SetValues (cross-domain boundary crossed here only)
            → FakeVehicleHardware (property updated, change event fired)
              → GRPCVehicleProxyServer (StartPropertyValuesStream push to vhal-bridge)
                → vhal-bridge GRPCVehicleHardware (stream event from 127.0.0.1)
                  → DefaultVehicleHal (property change callback)
                    → rvc_service GearSelectionMonitor (GEAR_SELECTION=REVERSE)
                      → CameraStreamManager.open() → H.264 RTP stream starts
```

Connection lifecycle logging (in GRPCVehicleProxyServer):
- "Client connected from X" — logged once on first RPC from a new peer address
- "Client disconnected from X" — logged when a streaming connection from that peer closes

vhal-gateway threading model:
- GRPCVehicleHardware runs a `ValuePollingLoop` thread (StartPropertyValuesStream)
- Each NodeForwarder has one worker thread — queues changed values and calls remote SetValues
- The callback thread (ValuePollingLoop) never blocks: forward() enqueues and returns immediately

---

## Transport Architecture Detail

```
Client side (vhal-ipc-grpc):           Server side (vhal-ipc-grpc):
  VehicleServer::Stub                    VehicleServer::Service
    ::GetValues()           ──wire──►      GRPCVehicleProxyServer::GetValues()
    ::SetValues()                          GRPCVehicleProxyServer::SetValues()
    ::StartPropertyValuesStream()          GRPCVehicleProxyServer::StartPropertyValuesStream()
```

---

## Folder Structure

```
vhal-core/
├── CMakeLists.txt              # Top-level build (single package today, three-package target)
├── conanfile.py                # Conan dependency manifest (workspace convenience)
├── stubs/                      # Linux replacements for Android-specific headers
├── scripts/
│   └── generate_aidl_headers.py  # Converts .aidl files to C++ headers
├── etc/
│   └── vhal/
│       └── vhalconfig/
│           └── DefaultProperties.json  # Initial property values
│
├── packages/
│   ├── vhal-proto/             # ← Conan header-library; proto files for external consumers
│   │   ├── conanfile.py        #   package_type="header-library", no settings
│   │   └── proto/              #   VehicleServer.proto + android/…/vehicle/*.proto
│   │
│   ├── vhal-types/             # [see Package Split above]
│   ├── vhal-ipc-grpc/          # [see Package Split above]
│   ├── vhal-server/            # [see Package Split above]
│   ├── vhal-gateway/           # [see Package Split above]
│   └── vhal-bridge/            # [see Package Split above]
│
├── aidl/                       # from hardware/interfaces/automotive/vehicle/aidl/
│   ├── android/                # AIDL interface definitions
│   ├── generated_lib/4/cpp/    # Pre-generated C++ property enum headers
│   └── impl/4/                 # V4 implementation (Android 16)
│       ├── default_config/     # Property JSON configs + JsonConfigLoader  [vhal-server]
│       ├── fake_impl/          # FakeVehicleHardware, GeneratorHub, OBD2   [vhal-server]
│       ├── grpc/               # GRPCVehicleProxyServer, ProtoMessageConverter [vhal-ipc-grpc]
│       │   ├── proto/          # VehicleServer.proto (gRPC service definition)
│       │   └── utils/proto_message_converter/
│       ├── hardware/           # IVehicleHardware (abstract interface)     [vhal-types]
│       ├── proto/              # Message .proto files                       [vhal-ipc-grpc]
│       │   └── android/hardware/automotive/vehicle/
│       ├── utils/common/       # VehiclePropertyStore, RecurrentTimer       [vhal-types]
│       └── vhal/               # Service entry point                        [vhal-server]
│
└── aidl_property/              # from hardware/interfaces/automotive/vehicle/aidl_property/
    └── android/hardware/automotive/vehicle/
                                # Full property enum universe (117 types)    [vhal-types]
```

---

## Key Design Decisions

| Decision | Detail |
| :--- | :--- |
| vhal-proto distribution | Proto text files packaged as a Conan `header-library` (`packages/vhal-proto/`) so external consumers (Velan, ClusterUI) declare `requires = "vhal-proto/1.0"` and resolve the proto directory at CMake configure time via `INTERFACE_INCLUDE_DIRECTORIES` — no hardcoded paths, portable across machines. |
| Domain isolation | Each node runs its own vhal-core server. Android clients never communicate across domain boundaries. vhal-gateway is the only cross-domain channel. |
| vhal-server on Android | packages/vhal-server/Android.bp builds `vhal-core-server` for the Android HU; listens on 0.0.0.0:50051 for gateway pushes and on 127.0.0.1:50051 for vhal-bridge |
| vhal-bridge connects locally | Default address is 127.0.0.1:50051 (same Android node). Never connects to the Linux IC. Override via vendor.vhal.grpc.server sysprop only if running a non-standard topology. |
| vhal-gateway role | Sole cross-domain channel on the Linux IC. Forwards a configured, filtered set of properties to each remote node's vhal-core server via gRPC SetValues. |
| Five-package split | vhal-types / vhal-ipc-grpc / vhal-server / vhal-gateway (Linux+Android) / vhal-bridge (Android only) — see Package Split above |
| vhal-gateway threading | One NodeForwarder thread per remote node; callback thread never blocked by remote I/O |
| vhal-gateway config | gateway-configs.json: version + array of { ipaddr, messages: [ { msgId, properties[] } ] }; source in packages/vhal-gateway/etc/vhal/ |
| Transport abstraction | IVehicleHardware decouples backend from transport |
| Current transport | gRPC replaces Binder; GRPCVehicleProxyServer is the server adapter |
| Planned transport | SOME/IP (vsomeip); entire vhal-ipc-grpc replaced, nothing else changes |
| Implementation version | aidl/impl/4/ — Android 16 = V4 |
| Property definitions | aidl_property/ — 117 vehicle property types |
| Android header stubs | stubs/ provides Linux replacements for Android-specific headers |
| AIDL → C++ | scripts/generate_aidl_headers.py; output under build/generated/aidl/ |
| Build system | CMake + Conan for Linux; Soong (Android.bp) for Android — one repo, two pipelines |
| Binder | Stubbed out on Linux — not used on the gRPC path |
| LargeParcelableBase | Stubbed as no-op — gRPC handles large messages natively |
| Include order guard | GRPCVehicleProxyServer.h must precede FakeVehicleHardware.h in any TU that includes both — grpc networking headers define TRY_AGAIN (netdb.h) which breaks the proto StatusCode enum |

---

## What Was Dropped from AOSP and Why

| Dropped Path | Reason |
| :--- | :--- |
| `aidl/aidl_api/` | AIDL versioning snapshots — Android build system only |
| `aidl/aidl_test/` | Android instrumentation tests, Java, HIDL compat tests |
| `aidl/emu_metadata/` | Android emulator metadata — not applicable |
| `aidl/generated_lib/4/java/` | Java stubs — not needed |
| `aidl/rust_impl/` | Rust VHAL implementation — wrong language for this port |
| `aidl/impl/4/vhal/src/fuzzer.cpp` | Android fuzzer harness |
| `aidl/impl/4/vhal/*.rc` | Android init.rc service definition |
| `aidl/impl/4/vhal/*.xml` | Android VINTF manifest |
| `aidl/impl/4/utils/test_vendor_properties/` | Test-only vendor AIDL |
| `aidl_property/aidl_api/` | Frozen version snapshots — Android build system only |
| `hardware/interfaces/automotive/vehicle/proto/` | HIDL V2.0 era proto — obsolete |
| `hardware/interfaces/automotive/vehicle/2.0/` | HIDL V2 implementation — obsolete |
| `packages/services/Car/cpp/vhal/` | Android-side Binder/AIDL client — replaced by gRPC stub |
| Sub-package `Android.bp` files | Verbatim AOSP copies that duplicate platform modules — deleted to prevent Soong "already defined" errors; only `vhal-server/Android.bp` and `vhal-bridge/Android.bp` are kept |

---

## What Was Kept and Why

| Kept Path | Reason |
| :--- | :--- |
| `aidl/android/` | AIDL interface definitions — reference and future code generation |
| `aidl/generated_lib/4/cpp/` | Pre-generated C++ property enum headers — used directly |
| `aidl/impl/4/proto/` | Message proto files — compiled into VehicleHalProtos |
| `aidl/impl/4/grpc/proto/` | VehicleServer.proto — gRPC service definition (transport contract) |
| `aidl/impl/4/grpc/` | Core gRPC transport implementation |
| `aidl/impl/4/hardware/` | IVehicleHardware abstract interface — backend abstraction |
| `aidl/impl/4/fake_impl/` | Simulation backend — development and testing |
| `aidl/impl/4/default_config/` | Property configs and JSON loader |
| `aidl/impl/4/utils/common/` | VehiclePropertyStore, RecurrentTimer, core utilities |
| `aidl/impl/4/vhal/` | Service entry point — Binder init removed, gRPC server substituted |
| `aidl_property/android/` | Full property enum universe — authoritative source of all car properties |
| `packages/vhal-server/aidl/impl/4/vhal/Android.bp` | Retained original Soong libs (DefaultVehicleHal, FakeVehicleHardware) — used by vhal-bridge and vhal-core-server |
