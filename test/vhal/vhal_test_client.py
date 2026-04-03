import argparse
import sys
import grpc
import VehicleServer_pb2
import VehicleServer_pb2_grpc
from android.hardware.automotive.vehicle import VehiclePropValue_pb2
from android.hardware.automotive.vehicle import VehiclePropValueRequest_pb2
from android.hardware.automotive.vehicle import StatusCode_pb2

SERVER_ADDR = "127.0.0.1:50051"

# ── Property IDs ──────────────────────────────────────────────────────────────
PERF_VEHICLE_SPEED      = 0x11600207  # float  m/s
ENGINE_RPM              = 0x11600305  # float  RPM
FUEL_LEVEL              = 0x11600307  # float  ml
ENGINE_COOLANT_TEMP     = 0x11600301  # float  °C
GEAR_SELECTION          = 0x11400400  # int32  VehicleGear
TURN_SIGNAL_LIGHT_STATE = 0x11400410  # int32  VehicleTurnSignal (replaces deprecated TURN_SIGNAL_STATE)

PROP_NAMES = {
    PERF_VEHICLE_SPEED:      "PERF_VEHICLE_SPEED",
    ENGINE_RPM:              "ENGINE_RPM",
    FUEL_LEVEL:              "FUEL_LEVEL",
    ENGINE_COOLANT_TEMP:     "ENGINE_COOLANT_TEMP",
    GEAR_SELECTION:          "GEAR_SELECTION",
    TURN_SIGNAL_LIGHT_STATE: "TURN_SIGNAL_LIGHT_STATE",
}

ALL_PROPS = [
    PERF_VEHICLE_SPEED,
    ENGINE_RPM,
    FUEL_LEVEL,
    ENGINE_COOLANT_TEMP,
    GEAR_SELECTION,
    TURN_SIGNAL_LIGHT_STATE,
]

# ── VehicleGear constants ─────────────────────────────────────────────────────
GEAR = {
    "neutral": 0x1, "reverse": 0x2, "park": 0x4, "drive": 0x8,
    "1": 0x10, "2": 0x20, "3": 0x40, "4": 0x80, "5": 0x100,
    "6": 0x200, "7": 0x400, "8": 0x800, "9": 0x1000,
}

# ── VehicleTurnSignal constants ───────────────────────────────────────────────
TURN = {"none": 0, "right": 1, "left": 2}


# ── Helpers ───────────────────────────────────────────────────────────────────

def status_name(code):
    try:
        return StatusCode_pb2.StatusCode.Name(code)
    except ValueError:
        return str(code)


def fmt_value(prop_id, val):
    if val.float_values:
        return str(val.float_values[0])
    if val.int32_values:
        v = val.int32_values[0]
        if prop_id == GEAR_SELECTION:
            name = {v: k for k, v in GEAR.items()}.get(v)
            return f"{name} ({v:#x})" if name else hex(v)
        if prop_id == TURN_SIGNAL_LIGHT_STATE:
            name = {v: k for k, v in TURN.items()}.get(v)
            return name if name else str(v)
        return str(v)
    return "<empty>"


def get_values(stub, prop_ids):
    requests = [
        VehiclePropValueRequest_pb2.VehiclePropValueRequest(
            request_id=i,
            value=VehiclePropValue_pb2.VehiclePropValue(prop=pid, area_id=0),
        )
        for i, pid in enumerate(prop_ids)
    ]
    batch = VehiclePropValueRequest_pb2.VehiclePropValueRequests(requests=requests)
    try:
        results = stub.GetValues(batch, timeout=5)
    except grpc.RpcError as e:
        print(f"ERROR: GetValues failed: {e.code()} — {e.details()}")
        sys.exit(1)
    return {prop_ids[r.request_id]: r for r in results.results}


def set_values(stub, requests):
    batch = VehiclePropValueRequest_pb2.VehiclePropValueRequests(requests=requests)
    try:
        results = stub.SetValues(batch, timeout=5)
    except grpc.RpcError as e:
        print(f"ERROR: SetValues failed: {e.code()} — {e.details()}")
        sys.exit(1)
    return {r.request_id: r for r in results.results}


def print_values(results):
    for pid in ALL_PROPS:
        name = PROP_NAMES[pid]
        res = results.get(pid)
        if res is None:
            print(f"  {name}: <not returned>")
            continue
        st = status_name(res.status)
        if res.status == StatusCode_pb2.OK and res.HasField("value"):
            print(f"  {name}: {fmt_value(pid, res.value)}")
        else:
            print(f"  {name}: {st}")


# ── main ──────────────────────────────────────────────────────────────────────

def parse_gear(s):
    sl = s.lower()
    if sl in GEAR:
        return GEAR[sl]
    try:
        return int(s, 0)
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"Invalid gear '{s}'. Use: {', '.join(GEAR)} or a raw integer."
        )


def parse_turn(s):
    sl = s.lower()
    if sl in TURN:
        return TURN[sl]
    try:
        return int(s, 0)
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"Invalid turn signal '{s}'. Use: {', '.join(TURN)} or a raw integer."
        )


def main():
    parser = argparse.ArgumentParser(description="VHAL gRPC test client")
    parser.add_argument("--server", default=SERVER_ADDR)
    parser.add_argument("--speed",       type=float,      metavar="M/S")
    parser.add_argument("--rpm",         type=float,      metavar="RPM")
    parser.add_argument("--fuel",        type=float,      metavar="LITRES")
    parser.add_argument("--temp",        type=float,      metavar="DEG_C")
    parser.add_argument("--gear",        type=parse_gear, metavar="GEAR",
                        help="park|reverse|neutral|drive|1-9 or raw int")
    parser.add_argument("--turn-signal", type=parse_turn, metavar="SIGNAL",
                        help="none|right|left or raw int")
    args = parser.parse_args()

    with grpc.insecure_channel(args.server) as channel:
        try:
            grpc.channel_ready_future(channel).result(timeout=5)
        except grpc.FutureTimeoutError:
            print(f"ERROR: Timed out connecting to {args.server}")
            sys.exit(1)

        stub = VehicleServer_pb2_grpc.VehicleServerStub(channel)

        # 1. Read current values
        print("--- Current values ---")
        print_values(get_values(stub, ALL_PROPS))

        # 2. Build injection list from provided args
        inject = []
        def float_req(pid, val):
            return VehiclePropValueRequest_pb2.VehiclePropValueRequest(
                request_id=len(inject),
                value=VehiclePropValue_pb2.VehiclePropValue(prop=pid, area_id=0, float_values=[val]),
            )
        def int32_req(pid, val):
            return VehiclePropValueRequest_pb2.VehiclePropValueRequest(
                request_id=len(inject),
                value=VehiclePropValue_pb2.VehiclePropValue(prop=pid, area_id=0, int32_values=[val]),
            )

        if args.speed       is not None: inject.append(float_req(PERF_VEHICLE_SPEED,  args.speed))
        if args.rpm         is not None: inject.append(float_req(ENGINE_RPM,          args.rpm))
        if args.fuel        is not None: inject.append(float_req(FUEL_LEVEL,          args.fuel))
        if args.temp        is not None: inject.append(float_req(ENGINE_COOLANT_TEMP, args.temp))
        if args.gear        is not None: inject.append(int32_req(GEAR_SELECTION,      args.gear))
        if args.turn_signal is not None: inject.append(int32_req(TURN_SIGNAL_LIGHT_STATE, args.turn_signal))

        if not inject:
            return

        # 3. Inject
        print("\n--- Injecting ---")
        set_results = set_values(stub, inject)
        for req in inject:
            pid = req.value.prop
            st  = status_name(set_results[req.request_id].status)
            print(f"  {PROP_NAMES[pid]}: {st}")

        # 4. Read back
        print("\n--- Values after injection ---")
        injected_pids = [req.value.prop for req in inject]
        print_values(get_values(stub, injected_pids))


if __name__ == "__main__":
    main()
