# vhal-core
The core of Android's VHAL, with gRPC, to be used in Linux and QNX.

# How to build
## Native / Linux
```
conan profile detect

# Install dependencies
conan install . --output-folder=build/Release --build=missing

# Configure
cmake -B build/Release \
    -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)
```

# Architecture & Intent
## Block diagram

```
┌─────────────────────────────────────────────────────┐
│                  ClusterUI / Client                 │
│         (Linux process or QNX process)              │
└──────────────────────┬──────────────────────────────┘
                       │ gRPC (proto-defined VHAL API)
                       │ or Unix Domain Socket
┌──────────────────────▼──────────────────────────────┐
│            VHAL Service (your port)                 │
│  ┌─────────────────────────────────────────────┐    │
│  │  VehiclePropertyStore  (AOSP logic, kept)   │    │
│  │  FakeVehicleHardware / OBD2 backend         │    │
│  └─────────────────────────────────────────────┘    │
│  Transport: gRPC server (replaces Binder/AIDL)      │
│                                                     │
│  Deps via Conan:                                    │
│    grpc, protobuf, jsoncpp, openssl, zlib           │
│  Deps built from AOSP source:                       │
│    libbase, libutils, libcutils (glibc-retargeted)  │
└─────────────────────────────────────────────────────┘
```


## Source Structure
This project is a Linux/QNX port of Android 16 VHAL using gRPC as the transport layer, replacing Android's Binder/AIDL IPC. The source is selectively extracted from AOSP at:
```
hardware/interfaces/automotive/vehicle/
```

### Folder Structure
```
vhal-core/
├── CMakeLists.txt
├── conanfile.py
├── README.md
│
├── aidl/                          # from hardware/interfaces/automotive/vehicle/aidl/
│   ├── android/                   # AIDL interface definitions (IVehicle, VehiclePropValue, etc.)
│   │   └── hardware/
│   │       └── automotive/
│   │           └── vehicle/
│   ├── generated_lib/
│   │   └── 4/
│   │       └── cpp/               # pre-generated C++ headers (VehicleProperty.h, etc.)
│   └── impl/
│       └── 4/                     # V4 implementation (Android 16 / "current")
│           ├── default_config/    # property JSON configs + JsonConfigLoader
│           ├── fake_impl/         # FakeVehicleHardware, GeneratorHub, OBD2, UserHal
│           ├── grpc/              # GRPCVehicleHardware, GRPCVehicleProxyServer
│           │   ├── proto/         # VehicleServer.proto (gRPC service definition)
│           │   └── utils/
│           │       └── proto_message_converter/
│           ├── hardware/          # IVehicleHardware (abstract interface, header-only)
│           ├── proto/             # message .proto files (wire format types)
│           │   └── android/
│           │       └── hardware/
│           │           └── automotive/
│           │               └── vehicle/
│           ├── utils/
│           │   └── common/        # VehiclePropertyStore, RecurrentTimer, etc.
│           └── vhal/              # service entry point (Binder init removed)
│
└── aidl_property/                 # from hardware/interfaces/automotive/vehicle/aidl_property/
    └── android/                   # property definition AIDL files (VehicleProperty.aidl, etc.)
        └── hardware/
            └── automotive/
                └── vehicle/
```

### What Was Dropped from AOSP and Why

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


### What Was Kept and Why

| Kept Path | Reason |
| :--- | :--- |
| `aidl/android/` | **AIDL interface definitions** — reference and future code generation |
| `aidl/generated_lib/4/cpp/` | **Pre-generated C++ property enum headers** — used directly |
| `aidl/impl/4/proto/` | **Message proto files** — compiled into `VehicleHalProtos` static lib |
| `aidl/impl/4/grpc/proto/` | **VehicleServer.proto** — gRPC service definition (the transport contract) |
| `aidl/impl/4/grpc/` | **Core gRPC transport implementation** |
| `aidl/impl/4/hardware/` | **IVehicleHardware abstract interface** — backend abstraction |
| `aidl/impl/4/fake_impl/` | **Simulation backend** — initial development and testing |
| `aidl/impl/4/default_config/` | **Property configs and JSON loader** |
| `aidl/impl/4/utils/common/` | **VehiclePropertyStore**, `RecurrentTimer`, core utilities |
| `aidl/impl/4/vhal/` | **Service entry point** — Binder init removed, gRPC server substituted |
| `aidl_property/android/` | **Full property enum universe** — authoritative source of all car properties |



### Transport Architecture
```
ClusterUI / any gRPC client
        │
        │  gRPC over TCP or Unix Domain Socket
        │  (VehicleServer.proto defines the service)
        ▼
GRPCVehicleProxyServer
        │
        ▼
FakeVehicleHardware  (simulation)
or
RealHardware backend (OBD2, CAN — future)
        │
        ▼
VehiclePropertyStore
Android Binder, libbinder, libbinder_ndk, libapexsupport, libvndksupport and Bionic libc are not used. All dependencies are standard open-source libraries managed via Conan.
```