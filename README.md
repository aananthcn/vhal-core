# vhal-core

Android 16 Vehicle HAL (VHAL) ported to Linux and QNX, with gRPC replacing Android's Binder/AIDL
transport. ClusterUI and other non-Android clients derive vehicle property data from this service.

For architecture details, design decisions, and folder structure see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Domain Isolation

Each node (Linux IC, Android HU) runs its own vhal-core server. Nodes never communicate
directly with each other's server — vhal-gateway is the only cross-domain channel.

```
Linux IC (192.168.10.10)                   Android HU (192.168.10.20)
═══════════════════════════                ══════════════════════════════════
vhal-core server (primary)                 vhal-core-server (local)
vhal-gateway ──gRPC SetValues────────────► (receives gateway pushes)
                                           vhal-bridge ──127.0.0.1:50051──► vhal-core-server
                                           rvc_service ──AIDL──► vhal-bridge
```

---

## Getting Started

vhal-core targets **two independent build pipelines** from the same repo:

| Pipeline | Target | Build system | Produces |
|---|---|---|---|
| Linux | Instrument Cluster (RPi OS, QNX) | CMake + Conan | `vhal-core` server + `vhal-gateway` daemon |
| Android | Head Unit / AAOS (RPi) | Soong (Android.bp) | `vhal-core-server` (local gRPC server) + `android.hardware.automotive.vehicle@V4-grpc-service` (AIDL bridge) |

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
# Detect the pc build profile (run once per machine)
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

The Android build produces two binaries that work together:
- **`vhal-core-server`** — a local gRPC server holding the Android node's property store.
  Receives property updates from the Linux IC via vhal-gateway.
- **`android.hardware.automotive.vehicle@V4-grpc-service`** (vhal-bridge) — the Android VHAL
  AIDL service. Connects to `vhal-core-server` at `127.0.0.1:50051`. Android clients
  (rvc_service, CarService) use this service via the normal AIDL binder interface.

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

#### Step 2 — Activate both packages in the device build

In your device's `device.mk`:

```makefile
# Remove the default fake VHAL
PRODUCT_PACKAGES -= android.hardware.automotive.vehicle@V4-default-service

# Add the local gRPC server and the AIDL bridge (both built from vendor/brcm/vhal-core)
PRODUCT_PACKAGES += vhal-core-server
PRODUCT_PACKAGES += android.hardware.automotive.vehicle@V4-grpc-service
```

#### Step 3 — Build

```bash
# From AOSP root — build vhal-core-server, vhal-bridge, and their dependencies:
source build/envsetup.sh
lunch aosp_rpi5_car-bp4a-userdebug
mmm vendor/brcm/vhal-core
```

Binaries produced:
- `out/target/product/rpi5/vendor/bin/vhal-core-server`
- `out/target/product/rpi5/vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service`

#### Step 4 — Configure vhal-gateway on the Linux IC

vhal-bridge connects to the local vhal-core-server at `127.0.0.1:50051` by default —
no Android-side configuration is needed.

On the Linux IC, configure vhal-gateway to forward property changes to the Android HU.
Edit `packages/vhal-gateway/etc/vhal/gateway-configs.json` (or the deployed copy at
`/opt/car-ui/etc/vhal/gateway-configs.json`) so that `ipaddr` points to the Android HU:

```json
{
    "version": "1.0",
    "gatewayNodes": [
        {
            "ipaddr": "192.168.10.20:50051",
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

Replace `192.168.10.20` with the actual Ethernet IP of the Android HU.

#### Step 5 — Push and test without reflashing

```bash
# One-time setup: disable dm-verity so /vendor can be remounted rw
adb root
adb disable-verity
adb reboot

# After reboot:
adb root && adb remount

# Push both binaries
adb push out/target/product/rpi5/vendor/bin/vhal-core-server \
         /vendor/bin/vhal-core-server
adb push out/target/product/rpi5/vendor/bin/hw/android.hardware.automotive.vehicle@V4-grpc-service \
         /vendor/bin/hw/

# Restart both services (vhal-core-server first — vhal-bridge depends on it)
adb shell stop vhal-core-server
adb shell start vhal-core-server
adb shell stop vhal-grpc
adb shell start vhal-grpc

adb logcat -s VhalBridge VehicleService
```

With vhal-core running on the Linux IC, vhal-gateway configured to forward to the Android HU,
and both Android binaries running, a property injected on the Linux IC reaches Android:

```bash
# On Linux IC — inject gear change to REVERSE (run from vhal-core repo root)
python test/vhal/vhal_test_client.py --gear reverse --server 192.168.10.10

# On Android — verify it arrived via gateway → local vhal-core-server → vhal-bridge
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
The remote node must be running its own vhal-core server to receive the forwarded values.

Config file format (`packages/vhal-gateway/etc/vhal/gateway-configs.json`):
```json
{
    "version": "1.0",
    "gatewayNodes": [
        {
            "ipaddr": "192.168.10.20:50051",
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
