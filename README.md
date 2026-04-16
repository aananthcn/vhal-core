# vhal-core

Android 16 Vehicle HAL (VHAL) ported to Linux and QNX, with gRPC replacing Android's Binder/AIDL
transport. ClusterUI and other non-Android clients derive vehicle property data from this service.

For architecture details, design decisions, and folder structure see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Getting Started

vhal-core targets **two independent build pipelines** from the same repo:

| Pipeline | Target | Build system | Produces |
|---|---|---|---|
| Linux | Instrument Cluster (RPi OS, QNX) | CMake + Conan | `vhal-core` server + `vhal-gateway` daemon |
| Android | Head Unit / AAOS (RPi5) | Soong (Android.bp) | `android.hardware.automotive.vehicle@V4-grpc-service` |

---

### Getting Started — Linux (Instrument Cluster)

#### Prerequisites

```bash
sudo apt install build-essential cmake python3-pip
pip3 install conan
```

#### Clone

```bash
git clone https://github.com/aananthcn/vhal-core.git ~/path/to/vhal-core
cd ~/path/to/vhal-core
```

#### Build

```bash
# Detect the host build profile (run once per machine)
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
- `build/Release/packages/vhal-server/vhal-core` — VHAL gRPC server
- `build/Release/packages/vhal-gateway/vhal-gateway` — property forwarding daemon

---

### Getting Started — Android (AAOS Head Unit)

vhal-bridge is the Android VHAL service built from this repo. It replaces the
default `FakeVehicleHardware`-backed VHAL with one that connects to the Linux
vhal-core gRPC server over Ethernet.

#### Step 1 — Place the repo in the AOSP vendor tree

Clone vhal-core directly into the Android source tree:

```bash
cd <AOSP_ROOT>
git clone https://github.com/aananthcn/vhal-core.git vendor/brcm/vhal-core
```

Or add it to your `repo` manifest so it is fetched automatically on `repo sync`:

```xml
<!-- In your device manifest (e.g. .repo/manifests/default.xml) -->
<project name="vhal-core"
         path="vendor/brcm/vhal-core"
         remote="<your-remote>"
         revision="main" />
```

#### Step 2 — Activate vhal-bridge in the device build

In your device's `device.mk`, swap out the default VHAL for the gRPC-backed one:

```makefile
# Remove the default fake VHAL
PRODUCT_PACKAGES -= android.hardware.automotive.vehicle@V4-default-service

# Add the gRPC bridge VHAL (built from vendor/brcm/vhal-core)
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V4-grpc-service
```

#### Step 3 — Build

```bash
# From AOSP root — build only vhal-bridge and its dependencies:
source build/envsetup.sh
lunch <your_target>
mmm vendor/brcm/vhal-core

# Or include it in a full build:
m android.hardware.automotive.vehicle@V4-grpc-service
```

Binary produced:
- `out/target/product/<device>/vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service`

#### Step 4 — Configure the server address

The service connects to `192.168.10.10:50051` by default (the Instrument Cluster
Ethernet IP). To change it without rebuilding, set a system property before the
service starts:

```bash
adb shell setprop vendor.vhal.grpc.server 192.168.10.10:50051
adb shell stop vhal-grpc
adb shell start vhal-grpc
```

Or edit `packages/vhal-bridge/vhal-grpc-service.rc` to pass the address as a
command-line argument to the binary and rebuild.

#### Step 5 — Push and test without reflashing

```bash
adb root && adb remount
adb push out/target/product/<device>/vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service \
         /vendor/bin/hw/
adb shell stop vhal-grpc
adb shell start vhal-grpc
adb logcat -s VhalBridge
```

With vhal-core running on the Linux IC and vhal-bridge running on Android, any
property injected via the Python test client is visible to both sides:

```bash
# On Linux IC — inject gear change to REVERSE
python src/vhal-core/test/vhal/vhal_test_client.py --gear reverse --server 192.168.10.10

# On Android — verify it arrived
adb shell cmd car_service get-property-value 0x11400400 0
```

---

## Run VHAL Server (Linux)

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

## Run VHAL Gateway (Linux)

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
