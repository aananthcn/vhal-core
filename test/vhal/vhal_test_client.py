import argparse
import sys
import grpc
import VehicleServer_pb2
import VehicleServer_pb2_grpc
from android.hardware.automotive.vehicle import VehiclePropValue_pb2
from android.hardware.automotive.vehicle import VehiclePropValueRequest_pb2
from android.hardware.automotive.vehicle import StatusCode_pb2

# Property IDs (VehiclePropertyGroup:SYSTEM | VehicleArea:GLOBAL | VehiclePropertyType:FLOAT)
PERF_VEHICLE_SPEED = 0x11600207  # Speedometer — m/s
ENGINE_RPM         = 0x11600305  # Tachometer  — RPM

PROP_NAMES = {
    PERF_VEHICLE_SPEED: "PERF_VEHICLE_SPEED",
    ENGINE_RPM:         "ENGINE_RPM",
}


def make_get_request(request_id, prop_id, area_id=0):
    prop_value = VehiclePropValue_pb2.VehiclePropValue(prop=prop_id, area_id=area_id)
    return VehiclePropValueRequest_pb2.VehiclePropValueRequest(
        request_id=request_id, value=prop_value
    )


def make_set_request(request_id, prop_id, float_val, area_id=0):
    prop_value = VehiclePropValue_pb2.VehiclePropValue(
        prop=prop_id, area_id=area_id, float_values=[float_val]
    )
    return VehiclePropValueRequest_pb2.VehiclePropValueRequest(
        request_id=request_id, value=prop_value
    )


def get_values(stub, requests):
    """Issue GetValues and return a dict of {request_id: GetValueResult}."""
    batch = VehiclePropValueRequest_pb2.VehiclePropValueRequests(requests=requests)
    try:
        results = stub.GetValues(batch, timeout=2)
    except grpc.RpcError as e:
        print(f"ERROR: GetValues RPC failed: {e.code()} — {e.details()}")
        sys.exit(1)
    return {r.request_id: r for r in results.results}


def set_values(stub, requests):
    """Issue SetValues and return a dict of {request_id: SetValueResult}."""
    batch = VehiclePropValueRequest_pb2.VehiclePropValueRequests(requests=requests)
    try:
        results = stub.SetValues(batch, timeout=2)
    except grpc.RpcError as e:
        print(f"ERROR: SetValues RPC failed: {e.code()} — {e.details()}")
        sys.exit(1)
    return {r.request_id: r for r in results.results}


def print_get_result(request_id, prop_id, result):
    name   = PROP_NAMES.get(prop_id, hex(prop_id))
    status = StatusCode_pb2.StatusCode.Name(result.status)
    if result.status == StatusCode_pb2.OK and result.HasField("value"):
        val = result.value.float_values[0] if result.value.float_values else "n/a"
        print(f"  {name}: {val}  (status={status})")
    else:
        print(f"  {name}: <no value>  (status={status})")


def print_set_result(request_id, prop_id, result):
    name   = PROP_NAMES.get(prop_id, hex(prop_id))
    status = StatusCode_pb2.StatusCode.Name(result.status)
    print(f"  {name}: {status}")


def run_test(speed, rpm):
    print("--- Starting VHAL gRPC test ---")
    with grpc.insecure_channel("127.0.0.1:50051") as channel:
        try:
            grpc.channel_ready_future(channel).result(timeout=5)
            print("Channel ready.")
        except grpc.FutureTimeoutError:
            print("ERROR: Timed out connecting to VHAL server at 127.0.0.1:50051")
            sys.exit(1)

        stub = VehicleServer_pb2_grpc.VehicleServerStub(channel)

        # --- Read current values ---
        print("\n--- Reading current values ---")
        get_reqs = [
            make_get_request(request_id=1, prop_id=PERF_VEHICLE_SPEED),
            make_get_request(request_id=2, prop_id=ENGINE_RPM),
        ]
        get_results = get_values(stub, get_reqs)
        print_get_result(1, PERF_VEHICLE_SPEED, get_results[1])
        print_get_result(2, ENGINE_RPM,         get_results[2])

        # --- Set new values ---
        print(f"\n--- Setting PERF_VEHICLE_SPEED to {speed} m/s, ENGINE_RPM to {rpm} RPM ---")
        set_reqs = [
            make_set_request(request_id=1, prop_id=PERF_VEHICLE_SPEED, float_val=speed),
            make_set_request(request_id=2, prop_id=ENGINE_RPM,         float_val=rpm),
        ]
        set_results = set_values(stub, set_reqs)
        print_set_result(1, PERF_VEHICLE_SPEED, set_results[1])
        print_set_result(2, ENGINE_RPM,         set_results[2])

        # --- Read back to confirm ---
        print("\n--- Reading back values after set ---")
        get_results = get_values(stub, get_reqs)
        print_get_result(1, PERF_VEHICLE_SPEED, get_results[1])
        print_get_result(2, ENGINE_RPM,         get_results[2])

    print("\n--- Test complete ---")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="VHAL gRPC test client")
    parser.add_argument("--speed", type=float, default=25.0,
                        help="Vehicle speed to set in m/s (default: 25.0)")
    parser.add_argument("--rpm",   type=float, default=3000.0,
                        help="Engine RPM to set (default: 3000.0)")
    args = parser.parse_args()
    run_test(args.speed, args.rpm)
