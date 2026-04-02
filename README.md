# vhal-core

Android 16 Vehicle HAL (VHAL) ported to Linux and QNX, with gRPC replacing Android's Binder/AIDL
transport. ClusterUI and other non-Android clients derive vehicle property data from this service.

For architecture details, design decisions, and folder structure see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Prerequisites

```bash
sudo apt install build-essential cmake python3-pip
pip3 install conan
```

---

## Build

```bash
# Detect the host build profile (run once)
conan profile detect

# Install dependencies
conan install . --output-folder=build/Release --build=missing

# Configure
cmake -B build/Release \
    -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build/Release -j$(nproc)
```

The binary is produced at `build/Release/vhal-core`.

---

## Run

```bash
./build/Release/vhal-core
```

Expected startup output:
```
I/VehicleService: Starting vhal-core gRPC server on 0.0.0.0:50051
I/FakeVehicleHardware: loading properties from .../etc/vhal/vhalconfig/
I/FakeVehicleHardware: loading properties from .../etc/vhal/vhalconfig/DefaultProperties.json
INFO/vhal: Wait: gRPC server is active and waiting for connections
```

The server listens on `0.0.0.0:50051` by default.

---

## Testing

See [test/README.md](test/README.md) for instructions on running the Python gRPC test client.

---

## Configuration

Initial property values are loaded from:
```
etc/vhal/vhalconfig/DefaultProperties.json
```

The optional power controller service config is read from:
```
etc/vhal/powercontroller/serverconfig
```
If the file is absent the server starts normally without power controller integration.
