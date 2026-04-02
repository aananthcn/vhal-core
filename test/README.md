# VHAL gRPC Test Client

A Python test client for `vhal-core` that reads and sets vehicle properties over gRPC.
The test targets two properties used by the instrument cluster:

| Property | ID | Unit |
| :--- | :--- | :--- |
| `PERF_VEHICLE_SPEED` | `0x11600207` | m/s |
| `ENGINE_RPM` | `0x11600305` | RPM |

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

# Default values: speed = 25.0 m/s, rpm = 3000.0
python3 vhal_test_client.py

# Custom values
python3 vhal_test_client.py --speed 33.3 --rpm 4500
```

### Options

| Argument | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `--speed` | float | `25.0` | Vehicle speed to set, in m/s |
| `--rpm` | float | `3000.0` | Engine RPM to set |

---

## Expected output

```
--- Starting VHAL gRPC test ---
Channel ready.

--- Reading current values ---
  PERF_VEHICLE_SPEED: 0.0  (status=OK)
  ENGINE_RPM: 0.0  (status=OK)

--- Setting PERF_VEHICLE_SPEED to 33.3 m/s, ENGINE_RPM to 4500.0 RPM ---
  PERF_VEHICLE_SPEED: OK
  ENGINE_RPM: OK

--- Reading back values after set ---
  PERF_VEHICLE_SPEED: 33.3  (status=OK)
  ENGINE_RPM: 4500.0  (status=OK)

--- Test complete ---
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
| :--- | :--- | :--- |
| `FutureTimeoutError` | Server not running | Start `./build/Release/vhal-core` first |
| `StatusCode.UNIMPLEMENTED` | Stale or wrong proto/pb2 files | Regenerate pb2 files (see above) |
| `ModuleNotFoundError: grpc` | grpcio not installed | `pip3 install grpcio grpcio-tools` |
| `NOT_AVAILABLE` status on set | Property is read-only in VHAL config | Check `DefaultProperties.json` access flags |
