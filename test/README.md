# VHAL gRPC Test Client

A Python test client for `vhal-core` that reads all vehicle properties over gRPC and
optionally injects values into any of the instrument-cluster properties.

| Property | ID | Type | Unit |
| :--- | :--- | :--- | :--- |
| `PERF_VEHICLE_SPEED` | `0x11600207` | float | m/s |
| `ENGINE_RPM` | `0x11600305` | float | RPM |
| `FUEL_LEVEL` | `0x11600309` | float | litres |
| `ENGINE_COOLANT_TEMP` | `0x11600303` | float | °C |
| `GEAR_SELECTION` | `0x11400400` | int32 | VehicleGear |
| `TURN_SIGNAL_STATE` | `0x11400408` | int32 | VehicleTurnSignal |

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
./build/Release/vhal-core
```

Wait until you see:
```
INFO/vhal: Wait: gRPC server is active and waiting for connections
```

**2. Run the test client** (in a separate terminal, from `test/vhal/`):

```bash
cd test/vhal

# Read all properties only — no injection
python3 vhal_test_client.py --read-only

# Inject speed and RPM (all other properties unchanged)
python3 vhal_test_client.py --speed 25.0 --rpm 3000.0

# Full cluster simulation
python3 vhal_test_client.py --speed 25.0 --rpm 3000.0 --fuel 30.0 --temp 90.0 --gear drive

# Gear and turn signal only
python3 vhal_test_client.py --gear reverse --turn-signal left

# Point at a remote server
python3 vhal_test_client.py --server 192.168.1.10:50051 --speed 10.0
```

### Options

| Argument | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `--server` | string | `127.0.0.1:50051` | vhal-core server address |
| `--read-only` | flag | — | Read all properties; skip injection |
| `--speed` | float | — | `PERF_VEHICLE_SPEED` in m/s (e.g. `25.0` ≈ 90 km/h) |
| `--rpm` | float | — | `ENGINE_RPM` in RPM |
| `--fuel` | float | — | `FUEL_LEVEL` in litres |
| `--temp` | float | — | `ENGINE_COOLANT_TEMP` in °C |
| `--gear` | string/int | — | `GEAR_SELECTION` — see table below |
| `--turn-signal` | string/int | — | `TURN_SIGNAL_STATE` — see table below |

Every injection argument is independent — omit any to leave that property unchanged.

#### `--gear` values

| Name | Raw value | Meaning |
| :--- | :--- | :--- |
| `neutral` | `0x1` | Neutral |
| `reverse` | `0x2` | Reverse |
| `park` | `0x4` | Park |
| `drive` | `0x8` | Drive |
| `1` … `9` | `0x10` … `0x1000` | Manual gear 1–9 |

A raw integer (decimal or hex) is also accepted: `--gear 0x8` is the same as `--gear drive`.

#### `--turn-signal` values

| Name | Raw value | Meaning |
| :--- | :--- | :--- |
| `none` | `0` | Off |
| `right` | `1` | Right indicator |
| `left` | `2` | Left indicator |

---

## Expected output

```
--- VHAL gRPC test  (127.0.0.1:50051) ---
Channel ready.

=== Reading all property configs ===
  42 properties configured on server.

    PropID        Status        Value                                     [access, change_mode]
------------------------------------------------------------------------------------------
  0x11400400  OK            int32s=[4]                                [READ_WRITE, ON_CHANGE]
  0x11400408  OK            int32s=[0]                                [READ, ON_CHANGE]
  0x11600207  OK            floats=[0.0]                              [READ, CONTINUOUS]
  ...

=== Injecting 3 value(s) ===
  PERF_VEHICLE_SPEED (0x11600207) = [25.0]
  ENGINE_RPM (0x11600305) = [3000.0]
  GEAR_SELECTION (0x11400400) = [8]

--- Set results ---
  PERF_VEHICLE_SPEED: OK
  ENGINE_RPM: OK
  GEAR_SELECTION: OK

--- Read-back ---
  PERF_VEHICLE_SPEED: floats=[25.0]  (status=OK)
  ENGINE_RPM: floats=[3000.0]  (status=OK)
  GEAR_SELECTION: int32s=[8]  (status=OK)

--- Done ---
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
| :--- | :--- | :--- |
| `FutureTimeoutError` | Server not running | Start `./build/Release/vhal-core` first |
| `StatusCode.UNIMPLEMENTED` | Stale or wrong proto/pb2 files | Regenerate pb2 files (see above) |
| `ModuleNotFoundError: grpc` | grpcio not installed | `pip3 install grpcio grpcio-tools` |
| `NOT_AVAILABLE` on set | Property is read-only | Check `access` field in `--read-only` output |
| `INVALID_ARG` on gear/turn | Value not in config array | Use `--read-only` to check `configArray` for valid values |
