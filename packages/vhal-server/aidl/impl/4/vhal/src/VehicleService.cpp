/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "VehicleService"

// Linux/gRPC port: Binder transport replaced by gRPC.
// The original VehicleService.cpp registered the HAL with Android's service
// manager. On Linux we instead expose it via GrpcVehicleProxyServer.

// GRPCVehicleProxyServer.h must come first: it pulls in VehicleServer.grpc.pb.h
// which includes StatusCode.pb.h before any grpc networking header gets a chance
// to #define TRY_AGAIN (a Linux netdb.h macro). FakeVehicleHardware.h also
// includes grpc++, but by then StatusCode.pb.h is already processed and guarded.
#include <GRPCVehicleProxyServer.h>
#include <FakeVehicleHardware.h>

#include <utils/Log.h>

#include <memory>
#include <string>

using ::android::hardware::automotive::vehicle::fake::FakeVehicleHardware;
using ::android::hardware::automotive::vehicle::virtualization::GrpcVehicleProxyServer;

static constexpr const char* kDefaultAddr = "0.0.0.0:50051";

int main(int argc, char* argv[]) {
    std::string addr = kDefaultAddr;
    if (argc > 1) {
        addr = argv[1];
    }

    ALOGI("Starting vhal-core gRPC server on %s", addr.c_str());

    auto hardware = std::make_unique<FakeVehicleHardware>();
    GrpcVehicleProxyServer server(addr, std::move(hardware));

    server.Start().Wait();

    ALOGI("vhal-core gRPC server exiting");
    return 0;
}
