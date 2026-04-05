# VHAL gRPC Test Client

A Python test client for `vhal-core` that reads all vehicle properties over gRPC and
optionally injects values into any of the instrument-cluster properties.

| Property | ID | Type | Unit |
| :--- | :--- | :--- | :--- |
| `PERF_VEHICLE_SPEED` | `0x11600207` | float | m/s |
| `ENGINE_RPM` | `0x11600305` | float | RPM |
| `FUEL_LEVEL` | `0x11600307` | float | millilitres |
| `ENGINE_COOLANT_TEMP` | `0x11600301` | float | °C |
| `GEAR_SELECTION` | `0x11400400` | int32 | VehicleGear |
| `PARKING_BRAKE_ON` | `0x11200402` | int32 | boolean (0=off, 1=on) |

---

## Prerequisites

Install the Python gRPC runtime and code-generation tools:

```bash
pip3 install grpcio grpcio-tools
```

---

## Directory layout

```
test/vhal/
├── VehicleServer.proto                         # gRPC service definition (copied from aidl/impl/4/grpc/proto/)
├── VehicleServer_pb2.py                        # generated — do not edit
├── VehicleServer_pb2_grpc.py                   # generated — do not edit
├── android/hardware/automotive/vehicle/        # message proto dependencies + generated pb2 files
└── vhal_test_client.py                         # test entry point
```

If the generated `_pb2.py` files are missing or out of date, regenerate them from `test/vhal/`:

```bash
cd test/vhal
python3 -m grpc_tools.protoc \
  -I. \
  --python_out=. \
  --grpc_python_out=. \
  VehicleServer.proto \
  android/hardware/automotive/vehicle/*.proto
```

---

## Running the test

**1. Start the vhal-core server** (from the repo root):

```bash
./build/Release/packages/vhal-server/vhal-core
```

Wait until you see:
```
INFO/vhal: Wait: gRPC server is ready to serve its clients!
```

**2. Run the test client** (in a separate terminal, from `test/vhal/`):

```bash
cd test/vhal

# Read current values only — no injection args means no write
python3 vhal_test_client.py

# Inject speed and RPM (all other properties unchanged)
python3 vhal_test_client.py --speed 25.0 --rpm 3000.0

# Full cluster simulation
python3 vhal_test_client.py --speed 25.0 --rpm 3000.0 --fuel 30000.0 --temp 90.0 --gear drive

# Gear and parking brake only
python3 vhal_test_client.py --gear reverse --park-brake on

# Point at a remote server
python3 vhal_test_client.py --server 192.168.1.10:50051 --speed 10.0
```

### Options

| Argument | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `--server` | string | `127.0.0.1:50051` | vhal-core server address |
| `--speed` | float | — | `PERF_VEHICLE_SPEED` in m/s (e.g. `25.0` ≈ 90 km/h) |
| `--rpm` | float | — | `ENGINE_RPM` in RPM |
| `--fuel` | float | — | `FUEL_LEVEL` in millilitres |
| `--temp` | float | — | `ENGINE_COOLANT_TEMP` in °C |
| `--gear` | string/int | — | `GEAR_SELECTION` — see table below |
| `--park-brake` | on/off | — | `PARKING_BRAKE_ON` — engaged or disengaged |

Every injection argument is independent — omit any to leave that property unchanged.

#### `--gear` values

Values are sourced from `VehicleGear` in `aidl/android/hardware/automotive/vehicle/VehicleGear.h`.

| Name | Raw value | `VehicleGear` constant |
| :--- | :--- | :--- |
| `neutral` | `0x0001` | `GEAR_NEUTRAL` |
| `reverse` | `0x0002` | `GEAR_REVERSE` |
| `park` | `0x0004` | `GEAR_PARK` |
| `drive` | `0x0008` | `GEAR_DRIVE` |
| `1` | `0x0010` | `GEAR_1` |
| `2` | `0x0020` | `GEAR_2` |
| `3` | `0x0040` | `GEAR_3` |
| `4` | `0x0080` | `GEAR_4` |
| `5` | `0x0100` | `GEAR_5` |
| `6` | `0x0200` | `GEAR_6` |
| `7` | `0x0400` | `GEAR_7` |
| `8` | `0x0800` | `GEAR_8` |
| `9` | `0x1000` | `GEAR_9` |

A raw integer (decimal or hex) is also accepted: `--gear 0x8` is the same as `--gear drive`.

#### `--park-brake` values

| Value | Meaning |
| :--- | :--- |
| `on`, `1`, `true` | Parking brake engaged |
| `off`, `0`, `false` | Parking brake disengaged |

---

## Expected output

```
--- Current values ---
  PERF_VEHICLE_SPEED: 0.0
  ENGINE_RPM: 0.0
  FUEL_LEVEL: 0.0
  ENGINE_COOLANT_TEMP: 0.0
  GEAR_SELECTION: park (0x4)
  PARKING_BRAKE_ON: off

--- Injecting ---
  PERF_VEHICLE_SPEED: OK
  ENGINE_RPM: OK
  GEAR_SELECTION: OK

--- Values after injection ---
  PERF_VEHICLE_SPEED: 25.0
  ENGINE_RPM: 3000.0
  GEAR_SELECTION: drive (0x8)
```

---

## Testing vhal-gateway (property forwarding)

This sequence verifies that the gateway reads property changes from one vhal-core instance
and forwards them to a second instance over gRPC.

### Setup

The default gateway config (`packages/vhal-gateway/etc/vhal/gateway-configs.json`) must
point at the destination node. For local two-instance testing, edit the file so that
`ipaddr` is `"localhost:50052"`:

```json
{
    "version": "1.0",
    "gatewayNodes": [
        {
            "ipaddr": "localhost:50052",
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

### Test sequence

**Terminal 1 — destination vhal-core (listens on 50052):**

```bash
./build/Release/packages/vhal-server/vhal-core 0.0.0.0:50052
```

Wait for:
```
INFO/vhal: Wait: gRPC server is ready to serve its clients!
```

**Terminal 2 — primary vhal-core (listens on 50051, default):**

```bash
./build/Release/packages/vhal-server/vhal-core
```

Wait for:
```
INFO/vhal: Wait: gRPC server is ready to serve its clients!
```

**Terminal 3 — gateway:**

```bash
./build/Release/packages/vhal-gateway/vhal-gateway
```

Expected startup output:
```
INFO/VhalGateway: [VhalGateway] local VHAL: localhost:50051
INFO/VhalGateway: [VhalGateway] config:     .../gateway-configs.json
INFO/VhalGateway: [VhalGateway] config v1.0 loaded — 1 node(s)
INFO/vhal: [NodeForwarder] started for localhost:50052 — 1 message group(s), 2 total property IDs
INFO/VhalGateway: [VhalGateway] connected, starting poll loop
```

**Terminal 4 — inject values into the primary:**

```bash
cd test/vhal
python3 vhal_test_client.py --gear drive --speed 25.0 --park-brake on
```

### Expected gateway behaviour

Immediately after injection the gateway detects the changed values and logs:

```
INFO/vhal: [VhalGateway] change detected for prop 0x11600207
INFO/vhal: [VhalGateway] change detected for prop 0x11400400
INFO/vhal: [VhalGateway] forwarding 2 changed prop(s)
INFO/vhal: [NodeForwarder] queuing [ivi-basic] → localhost:50052 (2 prop(s))
INFO/vhal: [NodeForwarder] sending [ivi-basic] → localhost:50052 (2 prop(s))
INFO/vhal: [NodeForwarder] SetValues [ivi-basic] to localhost:50052 OK
```

### Verify on the destination

```bash
cd test/vhal
python3 vhal_test_client.py --server localhost:50052
```

The read-back should show the same values that were injected into the primary:

```
--- Current values ---
  PERF_VEHICLE_SPEED: 25.0
  GEAR_SELECTION: drive (0x8)
  PARKING_BRAKE_ON: on
```

### Verify with tcpdump / Wireshark

To confirm gRPC frames are reaching the destination:

```bash
# tcpdump (terminal)
sudo tcpdump -i lo tcp port 50052 -n

# Wireshark
# Interface: lo (Loopback)
# Display filter: tcp.port == 50052
```

You should see TCP segments on port 50052 each time the gateway forwards a batch.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
| :--- | :--- | :--- |
| `FutureTimeoutError` | Server not running | Start `./build/Release/packages/vhal-server/vhal-core` first |
| `StatusCode.UNIMPLEMENTED` | Stale or wrong proto/pb2 files | Regenerate pb2 files (see above) |
| `ModuleNotFoundError: grpc` | grpcio not installed | `pip3 install grpcio grpcio-tools` |
| `NOT_AVAILABLE` on set | Property is read-only | Check `access` field in `--read-only` output |
| `INVALID_ARG` on gear/park-brake | Value not in config array | Run with no args to read current values and check `configArray` |
