import grpc
import vhal_pb2
import vhal_pb2_grpc

def run_test():
    print("--- Starting the VHAL gRPC test ---")
    # Connect to your Linux-ported VHAL server
    with grpc.insecure_channel('127.0.0.1:50051') as channel:
        try:
            print("--- gRPC Channel Ready Check ---")
            # Wait up to 5 seconds for the channel to be ready
            grpc.channel_ready_future(channel).result(timeout=5)
            stub = vhal_pb2_grpc.VehicleServiceStub(channel)

            print("--- Testing VHAL SetPropertyValue ---")
            # Example: Setting Prop ID 0x15800503 (HVAC_TEMPERATURE_SET) to 21.5°C
            target_value = vhal_pb2.VehiclePropValue(
                prop_id=0x15800503,
                area_id=0,
                float_value=21.5
            )
            set_request = vhal_pb2.SetValueRequest(value=target_value)
            status = stub.SetPropertyValue(set_request, timeout=2) # 2 second limit
            print(f"Set Status: {status.code}")

            print("\n--- Testing VHAL GetPropertyValue ---")
            get_request = vhal_pb2.GetValueRequest(prop_id=0x15800503, area_id=0)
            response = stub.GetPropertyValue(get_request, timeout=2) # 2 second limit
            
            print(f"Property ID: {hex(response.prop_id)}")
            print(f"Float Value: {response.float_value}")
            print(f"Timestamp: {response.timestamp}")
        except grpc.FutureTimeoutError:
            print("Error: System timed out connecting to VHAL server!")

if __name__ == "__main__":
    run_test()