# vhal-proto — Architecture

## What It Is

`vhal-proto` is a **Conan header-library package** that distributes the Android Vehicle HAL
protobuf/gRPC definition files as a versioned, portable dependency. It contains no compiled
code — consumers run `protoc` themselves with their own toolchain and gRPC plugin to generate
language-specific stubs from the proto files.

---

## Why It Exists

The canonical VHAL proto files live inside `packages/vhal-ipc-grpc/aidl/impl/4/`. External
consumers (Velan, ClusterUI, test scripts) previously depended on those files via **hardcoded
absolute paths**, which breaks reproducibility and cross-machine builds.

`vhal-proto` solves this by packaging the proto files as a first-class Conan dependency:

| Before | After |
|--------|-------|
| `set(VHAL_PROTO_DIR "/home/user/labs/networking/vhal-core/test/vhal")` | `find_package(vhal-proto CONFIG REQUIRED)` |
| Breaks on any machine that clones differently | Resolved by Conan on any machine |
| Not versioned | Tagged at `1.0`; bump version when proto ABI changes |

---

## Package Layout

```
packages/vhal-proto/
├── conanfile.py                              ← Conan recipe (header-library, no settings)
├── ARCHITECTURE.md                           ← this file
└── proto/                                    ← exported proto files
    ├── VehicleServer.proto                   ← gRPC service definition
    └── android/
        └── hardware/
            └── automotive/
                └── vehicle/
                    ├── DumpOptions.proto
                    ├── DumpResult.proto
                    ├── GetMinMaxSupportedValuesTypes.proto
                    ├── GetSupportedValuesListsTypes.proto
                    ├── HasSupportedValueInfo.proto
                    ├── PropIdAreaId.proto
                    ├── RawPropValues.proto
                    ├── StatusCode.proto
                    ├── SubscribeOptions.proto
                    ├── SubscribeRequest.proto
                    ├── SupportedValuesChange.proto
                    ├── UnsubscribeRequest.proto
                    ├── VehicleAreaConfig.proto
                    ├── VehiclePropConfig.proto
                    ├── VehiclePropertyAccess.proto
                    ├── VehiclePropertyChangeMode.proto
                    ├── VehiclePropertyStatus.proto
                    ├── VehiclePropValue.proto
                    └── VehiclePropValueRequest.proto
```

The files in `proto/` are copies of the canonical sources in
`packages/vhal-ipc-grpc/aidl/impl/4/`. The proto `package` declarations and import paths are
identical, so `--proto_path=<vhal-proto-include-dir>` is a drop-in replacement for
`--proto_path=<vhal-ipc-grpc>/aidl/impl/4/proto`.

---

## Conan Package Properties

| Property | Value |
|----------|-------|
| `package_type` | `header-library` |
| `no_copy_source` | `True` (proto files are plain text; no build step) |
| Settings | **None** — proto files are platform-independent text |
| CMake file name | `vhal-proto` |
| CMake target | `vhal-proto::vhal-proto` |
| Exposed include dir | `include/` — contains `VehicleServer.proto` at root and `android/…/*.proto` beneath |

No settings means the same Conan binary package is reused for both `pc` and `rpi` builds —
Conan caches it once regardless of architecture.

---

## Consumer Usage (CMake)

```cmake
find_package(vhal-proto CONFIG REQUIRED)

# Resolve the proto root directory from the Conan package
get_target_property(_dirs vhal-proto::vhal-proto INTERFACE_INCLUDE_DIRECTORIES)
list(GET _dirs 0 VHAL_PROTO_DIR)
message(STATUS "VHAL proto dir: ${VHAL_PROTO_DIR}")

# Use VHAL_PROTO_DIR as the --proto_path argument to protoc
add_custom_command(
    OUTPUT  gen/VehicleServer.pb.cc gen/VehicleServer.grpc.pb.cc ...
    COMMAND protoc
            --proto_path=${VHAL_PROTO_DIR}
            --cpp_out=gen/
            --grpc_out=gen/
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${VHAL_PROTO_DIR}/VehicleServer.proto
    ...
)
```

---

## Creating the Package

```bash
# One-time (or when proto files change); result is cached in ~/.conan2
conan create ~/labs/networking/vhal-core/packages/vhal-proto --version 1.0
```

Consumers that use `build_velan.sh` have this step run automatically before
`conan install`.

---

## Relationship to vhal-ipc-grpc

`vhal-proto` and `vhal-ipc-grpc` expose the **same proto files** but serve different
consumers:

| Package | Audience | What they get |
|---------|----------|---------------|
| `vhal-ipc-grpc` | vhal-server, vhal-gateway — compile proto → C++ stubs → link | Compiled static libs (`libVehicleHalProtos`, `libVehicleServerProtoStub`, etc.) |
| `vhal-proto` | External projects (Velan, ClusterUI) — run protoc themselves | Proto text files only; consumer picks their own toolchain |

External projects should depend on `vhal-proto`, not on `vhal-ipc-grpc`, because:
- `vhal-ipc-grpc` pulls in gRPC, vhal-types, and the full vhal-core build chain
- `vhal-proto` has zero dependencies — it is purely text files
