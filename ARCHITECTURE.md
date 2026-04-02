# vhal-core — Architecture

## Overview

vhal-core is a Linux/QNX port of the Android 16 Vehicle HAL (VHAL). It replaces Android's
Binder/AIDL IPC transport with gRPC, allowing any non-Android client (e.g. ClusterUI running
on Linux or QNX) to read and write vehicle properties over a standard TCP or Unix Domain Socket.

Source is selectively extracted from AOSP at:
```
hardware/interfaces/automotive/vehicle/
```

The implementation version is **V4**, corresponding to Android 16.

---

## System Block Diagram

```
┌─────────────────────────────────────────────────────┐
│                  ClusterUI / Client                 │
│         (Linux process or QNX process)              │
└──────────────────────┬──────────────────────────────┘
                       │ gRPC (VehicleServer.proto)
                       │ TCP or Unix Domain Socket
┌──────────────────────▼──────────────────────────────┐
│              GRPCVehicleProxyServer                 │
│  ┌─────────────────────────────────────────────┐    │
│  │        FakeVehicleHardware (sim)            │    │
│  │        or RealHardware backend (future)     │    │
│  ├─────────────────────────────────────────────┤    │
│  │         VehiclePropertyStore                │    │
│  └─────────────────────────────────────────────┘    │
│                                                     │
│  Dependencies via Conan:                            │
│    grpc, protobuf, jsoncpp, openssl, zlib           │
│  Dependencies built from AOSP source:               │
│    libbase, libutils, libcutils (glibc-retargeted)  │
└─────────────────────────────────────────────────────┘
```

---

## Transport Architecture

```
ClusterUI / any gRPC client
        │
        │  gRPC over TCP (default: 0.0.0.0:50051)
        │  or Unix Domain Socket
        │  (service contract: VehicleServer.proto)
        ▼
GRPCVehicleProxyServer          ← aidl/impl/4/grpc/
        │
        ▼
IVehicleHardware (abstract)     ← aidl/impl/4/hardware/
        │
        ├── FakeVehicleHardware  (simulation, default)
        │       loads DefaultProperties.json
        │
        └── RealHardware (OBD2 / CAN — future)
        │
        ▼
VehiclePropertyStore            ← aidl/impl/4/utils/common/
```

Android Binder, libbinder, libbinder_ndk, libapexsupport, libvndksupport, and Bionic libc are
not used. All dependencies are standard open-source libraries managed via Conan.

---

## Folder Structure

```
vhal-core/
├── CMakeLists.txt              # Top-level build definition
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
│   ├── android/                # AIDL interface definitions (IVehicle, VehiclePropValue, etc.)
│   ├── generated_lib/4/cpp/    # Pre-generated C++ property enum headers
│   └── impl/4/                 # V4 implementation (Android 16 / "current")
│       ├── default_config/     # Property JSON configs + JsonConfigLoader
│       ├── fake_impl/          # FakeVehicleHardware, GeneratorHub, OBD2, UserHal
│       ├── grpc/               # GRPCVehicleHardware, GRPCVehicleProxyServer
│       │   ├── proto/          # VehicleServer.proto (gRPC service definition)
│       │   └── utils/proto_message_converter/
│       ├── hardware/           # IVehicleHardware (abstract interface, header-only)
│       ├── proto/              # Message .proto files (wire format types)
│       │   └── android/hardware/automotive/vehicle/
│       ├── utils/common/       # VehiclePropertyStore, RecurrentTimer, etc.
│       └── vhal/               # Service entry point (Binder init removed)
│
└── aidl_property/              # from hardware/interfaces/automotive/vehicle/aidl_property/
    └── android/hardware/automotive/vehicle/
                                # Full property enum universe (VehicleProperty.aidl, etc.)
```

---

## Key Design Decisions

| Decision | Detail |
| :--- | :--- |
| Transport | gRPC replaces Binder. `GRPCVehicleProxyServer` is the entry point. |
| Implementation version | `aidl/impl/4/` — Android 16 = V4 |
| Property definitions | `aidl_property/` contains `VehicleProperty.aidl` and all enum types |
| Android header stubs | `stubs/` provides Linux replacements for Android-specific headers |
| AIDL → C++ | `scripts/generate_aidl_headers.py` converts `.aidl` files to C++ headers |
| Build system | All `Android.bp` (Soong) files replaced by `CMakeLists.txt` + Conan |
| Binder | Stubbed out — not used on the gRPC path |
| LargeParcelableBase | Stubbed as no-op — gRPC handles large messages natively |

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
| `packages/services/Car/cpp/vhal/` | Android-side Binder/AIDL client library — replaced by gRPC stub |
| `All Android.bp files` | Soong build system — replaced by CMakeLists.txt |

---

## What Was Kept and Why

| Kept Path | Reason |
| :--- | :--- |
| `aidl/android/` | AIDL interface definitions — reference and future code generation |
| `aidl/generated_lib/4/cpp/` | Pre-generated C++ property enum headers — used directly |
| `aidl/impl/4/proto/` | Message proto files — compiled into `VehicleHalProtos` static lib |
| `aidl/impl/4/grpc/proto/` | `VehicleServer.proto` — gRPC service definition (transport contract) |
| `aidl/impl/4/grpc/` | Core gRPC transport implementation |
| `aidl/impl/4/hardware/` | `IVehicleHardware` abstract interface — backend abstraction |
| `aidl/impl/4/fake_impl/` | Simulation backend — development and testing |
| `aidl/impl/4/default_config/` | Property configs and JSON loader |
| `aidl/impl/4/utils/common/` | `VehiclePropertyStore`, `RecurrentTimer`, core utilities |
| `aidl/impl/4/vhal/` | Service entry point — Binder init removed, gRPC server substituted |
| `aidl_property/android/` | Full property enum universe — authoritative source of all car properties |
