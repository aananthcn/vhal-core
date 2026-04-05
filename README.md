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

Binaries produced:
- `build/Release/packages/vhal-server/vhal-core` — VHAL server
- `build/Release/packages/vhal-gateway/vhal-gateway` — property forwarding daemon

---

## Run VHAL Server

```bash
./build/Release/packages/vhal-server/vhal-core 
./build/Release/packages/vhal-server/vhal-core 0.0.0.0:50052
```

Expected startup output:
```
I/VehicleService: Starting vhal-core gRPC server on 0.0.0.0:50051
I/FakeVehicleHardware: loading properties from .../etc/vhal/vhalconfig/
I/FakeVehicleHardware: loading properties from .../etc/vhal/vhalconfig/DefaultProperties.json
INFO/vhal: Wait: gRPC server is ready to serve its clients!
```

The server listens on `0.0.0.0:50051` by default.

---

## Run VHAL Gateway

```bash
# Uses default config: packages/vhal-gateway/etc/vhal/gateway-configs.json
./build/Release/packages/vhal-gateway/vhal-gateway

# Custom local VHAL address and config file
./build/Release/packages/vhal-gateway/vhal-gateway localhost:50051 /path/to/gateway-configs.json
```

The gateway connects to the local vhal-core, subscribes to all property change events,
and forwards matching properties to each configured remote node on change.

Config file format (`packages/vhal-gateway/etc/vhal/gateway-configs.json`):
```json
{
    "version": "1.0",
    "gatewayNodes": [
        {
            "ipaddr": "192.168.1.20:50051",
            "messages": [
                {
                    "msgId": "ivi-basic",
                    "properties": [
                        {"id": "0x11400400", "desc": "GEAR_SELECTION"},
                        {"id": "0x11600207", "desc": "PERF_VEHICLE_SPEED"}
                    ]
                }
            ]
        }
    ]
}
```
Each `messages` entry is a named group of properties. Property IDs are hex strings matching
the VHAL property enum values. When any property in a group changes, that group's changed
values are sent as a single `SetValues` call. Groups with no changed properties are not sent.

On final integration, deploy config to `/opt/car-ui/etc/vhal/gateway-configs.json`
(the default `GATEWAY_CONFIG_ROOT` for production builds).

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
