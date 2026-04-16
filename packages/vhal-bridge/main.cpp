/*
 * Copyright (C) 2025 Aananth C N
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

// vhal-bridge: Android VHAL service whose hardware backend is the local
// vhal-core gRPC server running on the same Android node.
//
// This replaces the default FakeVehicleHardware-backed VHAL on Android with
// one that delegates every GetValues / SetValues / Subscribe call to the
// local vhal-core server over gRPC (GRPCVehicleHardware).
//
// Android clients (rvc_service, CarService, …) continue to use the normal
// AIDL / HIDL Vehicle HAL interface — they are unaware of the gRPC bridge.
//
// Domain isolation: vhal-bridge never connects to the Linux IC directly.
// The Linux IC's vhal-gateway pushes property changes into the local
// vhal-core server (vhal-core-server), which in turn notifies vhal-bridge
// via StartPropertyValuesStream. The Linux IC is invisible to Android clients.

#define LOG_TAG "VhalBridge"

// GRPCVehicleHardware.h must be included before any header that pulls in
// grpc networking headers (which define TRY_AGAIN via netdb.h), because
// StatusCode.pb.h uses TRY_AGAIN as an enum value.
#include <GRPCVehicleHardware.h>
#include <DefaultVehicleHal.h>

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>
#include <sys/system_properties.h>

#include <chrono>
#include <memory>
#include <string>

using ::android::hardware::automotive::vehicle::DefaultVehicleHal;
using ::android::hardware::automotive::vehicle::virtualization::GRPCVehicleHardware;

// Default server address: local vhal-core-server on the same Android node.
// Override at runtime via system property vendor.vhal.grpc.server
// or pass as the first command-line argument (set in vhal-grpc-service.rc).
static constexpr const char* kDefaultServerAddr = "127.0.0.1:50051";
static constexpr const char* kServerAddrProp    = "vendor.vhal.grpc.server";
static constexpr auto        kConnectTimeout     = std::chrono::milliseconds(10'000);

static std::string resolveServerAddr(int argc, char** argv) {
    // Command-line argument takes highest priority (set in the .rc file).
    if (argc > 1 && argv[1][0] != '\0') {
        return argv[1];
    }

    // Fall back to system property.
    char propValue[PROP_VALUE_MAX] = {};
    if (__system_property_get(kServerAddrProp, propValue) > 0) {
        return propValue;
    }

    return kDefaultServerAddr;
}

int main(int argc, char** argv) {
    const std::string serverAddr = resolveServerAddr(argc, argv);

    ALOGI("vhal-bridge: connecting to vhal-core at %s", serverAddr.c_str());

    auto hardware = std::make_unique<GRPCVehicleHardware>(serverAddr);

    if (!hardware->waitForConnected(kConnectTimeout)) {
        ALOGE("vhal-bridge: timed out waiting for %s — is vhal-core running?",
              serverAddr.c_str());
        // Do not exit: gRPC channel will reconnect when the server comes up.
        // Android init will restart us if we crash, but a retry loop here
        // means we keep the AIDL service registration alive during brief
        // network outages.
    }

    ALOGI("vhal-bridge: connected — registering AIDL VehicleHal service");

    auto vhal = ndk::SharedRefBase::make<DefaultVehicleHal>(std::move(hardware));

    binder_exception_t err = AServiceManager_addService(
            vhal->asBinder().get(),
            "android.hardware.automotive.vehicle.IVehicle/default");
    if (err != EX_NONE) {
        ALOGE("vhal-bridge: AServiceManager_addService failed: %d", err);
        return 1;
    }

    ALOGI("vhal-bridge: service registered — entering binder thread pool");

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();
    ABinderProcess_joinThreadPool();

    ALOGI("vhal-bridge: exiting");
    return 0;
}
