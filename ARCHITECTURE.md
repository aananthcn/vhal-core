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

## Package Split

```
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
└─────────────┬──────────────┘
              │ depends on
┌─────────────▼──────────────┐
│  vhal-server               │
│  (runnable process)        │
│                            │
│  FakeVehicleHardware       │
│  DefaultProperties.json    │
│  main() / VehicleService   │
└────────────────────────────┘
```

**Client dependency (ClusterUI, etc.):**
```
ClusterUI → vhal-types + vhal-ipc-grpc (VehicleServer::Stub only)
```
ClusterUI has no compile-time dependency on vhal-server or GRPCVehicleProxyServer.
The server-side class and the client-side Stub both come from the same generated
`VehicleServer.grpc.pb.h`, but only the Stub is used by clients.

---

## IPC Transport Swap Path (gRPC → SOME/IP)

When SOME/IP is adopted:

| Layer | Change required |
| :--- | :--- |
| vhal-types | None — property IDs, value types, IVehicleHardware unchanged |
| vhal-ipc-grpc | Replaced entirely by vhal-ipc-someip |
| vhal-server | Relink against vhal-ipc-someip; no domain logic changes |
| ClusterUI | Swap VhalGrpcClient for VhalSomeIPClient; property access code unchanged |

---

## Runtime Block Diagram

```
┌─────────────────────────────────────────┐
│         ClusterUI / any client          │
│  VhalGrpcClient                         │
│    └── VehicleServer::Stub              │  ← from vhal-ipc-grpc
└───────────────────┬─────────────────────┘
                    │ gRPC over TCP (default: 0.0.0.0:50051)
                    │ or Unix Domain Socket
                    │ wire contract: VehicleServer.proto
┌───────────────────▼─────────────────────┐
│         vhal-core server process        │
│                                         │
│  GRPCVehicleProxyServer                 │  ← vhal-ipc-grpc
│    └── implements VehicleServer::Service│
│          │                              │
│          ▼                              │
│  IVehicleHardware (abstract)            │  ← vhal-types
│    └── FakeVehicleHardware (default)    │  ← vhal-server
│          └── VehiclePropertyStore       │  ← vhal-types
│          └── DefaultProperties.json     │
│          └── GeneratorHub (sim values)  │
└─────────────────────────────────────────┘
```

Connection lifecycle logging (in GRPCVehicleProxyServer):
- "Client connected from X" — logged once on first RPC from a new peer address
- "Client disconnected from X" — logged when a streaming connection from that peer closes

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
├── conanfile.py                # Conan dependency manifest
├── stubs/                      # Linux replacements for Android-specific headers
├── scripts/
│   └── generate_aidl_headers.py  # Converts .aidl files to C++ headers
├── etc/
│   └── vhal/
│       └── vhalconfig/
│           └── DefaultProperties.json  # Initial property values
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
| Three-package split | vhal-types / vhal-ipc-grpc / vhal-server — see Package Split above |
| Transport abstraction | IVehicleHardware decouples backend from transport |
| Current transport | gRPC replaces Binder; GRPCVehicleProxyServer is the server adapter |
| Planned transport | SOME/IP (vsomeip); entire vhal-ipc-grpc replaced, nothing else changes |
| Implementation version | aidl/impl/4/ — Android 16 = V4 |
| Property definitions | aidl_property/ — 117 vehicle property types |
| Android header stubs | stubs/ provides Linux replacements for Android-specific headers |
| AIDL → C++ | scripts/generate_aidl_headers.py; output under build/generated/aidl/ |
| Build system | All Android.bp (Soong) replaced by CMakeLists.txt + Conan |
| Binder | Stubbed out — not used on the gRPC path |
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
| `All Android.bp files` | Soong build system — replaced by CMakeLists.txt |

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
