cmake_minimum_required(VERSION 3.22)
project(vhal-core LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────────────────────
# Global settings
# ──────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# External dependencies (provided by Conan)
# Run: conan install . --output-folder=build --build=missing
#      cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
# ──────────────────────────────────────────────────────────────────────────────
find_package(protobuf REQUIRED CONFIG)
find_package(gRPC     REQUIRED CONFIG)
find_package(jsoncpp  REQUIRED CONFIG)

# ──────────────────────────────────────────────────────────────────────────────
# Protoc and gRPC plugin executables (from Conan-provided gRPC package)
# ──────────────────────────────────────────────────────────────────────────────
get_target_property(PROTOC_EXE       protobuf::protoc        IMPORTED_LOCATION)
get_target_property(GRPC_CPP_PLUGIN  gRPC::grpc_cpp_plugin   IMPORTED_LOCATION)

# ──────────────────────────────────────────────────────────────────────────────
# Source root paths
# ──────────────────────────────────────────────────────────────────────────────
set(IMPL_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4)
set(PROTO_SRC_DIR   ${IMPL_DIR}/proto)
set(GRPC_SRC_DIR    ${IMPL_DIR}/grpc)
set(HARDWARE_DIR    ${IMPL_DIR}/hardware)
set(UTILS_DIR       ${IMPL_DIR}/utils/common)
set(JSONCFG_DIR     ${IMPL_DIR}/default_config/JsonConfigLoader)
set(OBD2_DIR        ${IMPL_DIR}/fake_impl/obd2frame)
set(USERHAL_DIR     ${IMPL_DIR}/fake_impl/userhal)
set(GENHUB_DIR      ${IMPL_DIR}/fake_impl/GeneratorHub)
set(FAKEHW_DIR      ${IMPL_DIR}/fake_impl/hardware)
set(VHAL_DIR        ${IMPL_DIR}/vhal)
set(GENERATED_HDR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/aidl/generated_lib/4/cpp)

# ──────────────────────────────────────────────────────────────────────────────
# Generated file output directories
# ──────────────────────────────────────────────────────────────────────────────
set(PROTO_GEN_DIR   ${CMAKE_BINARY_DIR}/generated/proto)
set(GRPC_GEN_DIR    ${CMAKE_BINARY_DIR}/generated/grpc)

file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
file(MAKE_DIRECTORY ${GRPC_GEN_DIR})


# ══════════════════════════════════════════════════════════════════════════════
# AIDL LAYER 0: Generate C++ headers from aidl_property/ .aidl files
#
# Uses the AOSP aidl host tool to generate:
#   aidl/android/hardware/automotive/vehicle/Foo.h
# for every Foo.aidl in aidl_property/
#
# Output lands in:
#   ${CMAKE_BINARY_DIR}/generated/aidl/
# which is added as an include path to VehicleHalUtils and all consumers.
# ══════════════════════════════════════════════════════════════════════════════

# AIDL LAYER 0 — generate headers via Python script
find_package(Python3 REQUIRED)

set(AIDL_GEN_DIR ${CMAKE_BINARY_DIR}/generated)

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_aidl_headers.py
            --input  ${CMAKE_CURRENT_SOURCE_DIR}/aidl_property
            --output ${AIDL_GEN_DIR}
    RESULT_VARIABLE AIDL_GEN_RESULT1
)
if(NOT AIDL_GEN_RESULT1 EQUAL 0)
    message(FATAL_ERROR "AIDL header generation failed")
endif()

# Also generate headers from aidl/android/ (interface types)
execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_aidl_headers.py
            --input  ${CMAKE_CURRENT_SOURCE_DIR}/aidl
            --output ${AIDL_GEN_DIR}
    RESULT_VARIABLE AIDL_GEN_RESULT2
)
if(NOT AIDL_GEN_RESULT2 EQUAL 0)
    message(FATAL_ERROR "AIDL interface header generation failed")
endif()


add_library(AidlPropertyHeaders INTERFACE)
target_include_directories(AidlPropertyHeaders INTERFACE ${AIDL_GEN_DIR})


# ──────────────────────────────────────────────────────────────────────────────
# Stub headers — Android-specific headers replaced with Linux-compatible stubs
# ──────────────────────────────────────────────────────────────────────────────
set(STUBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

add_library(AndroidStubs INTERFACE)
target_include_directories(AndroidStubs INTERFACE ${STUBS_DIR})



# ══════════════════════════════════════════════════════════════════════════════
# LAYER 5   VehicleHalUtils
# Mirrors:  cc_library { name: "VehicleHalUtils" }
# Source:   aidl/impl/4/utils/common/
#
# Bottom of the dependency graph — no external library dependencies.
#
# Porting note:
#   include/ParcelableUtils.h references Android Parcel/Binder types.
#   Guard with #ifdef __ANDROID__ or provide a stub header when
#   compilation errors are encountered.
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB UTILS_SRCS "${UTILS_DIR}/src/*.cpp")

add_library(VehicleHalUtils STATIC ${UTILS_SRCS})

target_include_directories(VehicleHalUtils
    PUBLIC
		${UTILS_DIR}/include
		${STUBS_DIR}
)

target_compile_options(VehicleHalUtils PRIVATE -Wno-unused-parameter)

target_link_libraries(VehicleHalUtils
    PUBLIC
        AidlPropertyHeaders         # aidl/ generated headers
        AndroidStubs                # thread_annotations.h, utils/Log.h stubs
)

# ──────────────────────────────────────────────────────────────────────────────
# LAYER 5b  VehicleHalUtilHeaders  (header-only)
# Mirrors:  cc_library_headers { name: "VehicleHalUtilHeaders" }
# Consumers that need only the headers without linking the full static lib.
# ──────────────────────────────────────────────────────────────────────────────
add_library(VehicleHalUtilHeaders INTERFACE)

target_include_directories(VehicleHalUtilHeaders INTERFACE
    ${UTILS_DIR}/include
)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 2b  IVehicleHardware  (header-only)
# Mirrors:  cc_library_headers { name: "IVehicleHardware" }
# Source:   aidl/impl/4/hardware/include/IVehicleHardware.h
# Abstract backend interface — all hardware implementations derive from this.
# ══════════════════════════════════════════════════════════════════════════════
add_library(IVehicleHardware INTERFACE)

target_include_directories(IVehicleHardware INTERFACE
    ${HARDWARE_DIR}/include
)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 6p  IVehicleGeneratedHeaders  (header-only)
# Mirrors:  cc_library_headers { name: "IVehicleGeneratedHeaders-V4" }
# Source:   aidl/generated_lib/4/cpp/
# Exposes pre-generated property metadata headers:
#   AccessForVehicleProperty.h, ChangeModeForVehicleProperty.h,
#   EnumForVehicleProperty.h, AnnotationsForVehicleProperty.h,
#   VersionForVehicleProperty.h
# ══════════════════════════════════════════════════════════════════════════════
add_library(IVehicleGeneratedHeaders INTERFACE)

target_include_directories(IVehicleGeneratedHeaders INTERFACE
    ${GENERATED_HDR_DIR}
)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 1   VehicleHalProtos
# Mirrors:  cc_library_static { name: "VehicleHalProtos" }
# Source:   aidl/impl/4/proto/android/hardware/automotive/vehicle/*.proto
#
# All message .proto files compiled together.
# --proto_path points to proto/ root so imports resolve as:
#   "android/hardware/automotive/vehicle/Foo.proto"
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB_RECURSE VEHICLE_PROTO_FILES
    "${PROTO_SRC_DIR}/android/hardware/automotive/vehicle/*.proto"
)

set(VEHICLE_PROTO_SRCS "")
set(VEHICLE_PROTO_HDRS "")

foreach(PROTO_FILE ${VEHICLE_PROTO_FILES})
    file(RELATIVE_PATH REL_PROTO ${PROTO_SRC_DIR} ${PROTO_FILE})
    string(REPLACE ".proto" "" STEM ${REL_PROTO})

    set(PB_CC  ${PROTO_GEN_DIR}/${STEM}.pb.cc)
    set(PB_H   ${PROTO_GEN_DIR}/${STEM}.pb.h)

    add_custom_command(
        OUTPUT  ${PB_CC} ${PB_H}
        COMMAND ${PROTOC_EXE}
                --proto_path=${PROTO_SRC_DIR}
                --cpp_out=${PROTO_GEN_DIR}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Compiling proto: ${REL_PROTO}"
    )

    list(APPEND VEHICLE_PROTO_SRCS ${PB_CC})
    list(APPEND VEHICLE_PROTO_HDRS ${PB_H})
endforeach()

add_library(VehicleHalProtos STATIC ${VEHICLE_PROTO_SRCS})

target_include_directories(VehicleHalProtos
    PUBLIC  ${PROTO_GEN_DIR}
)

target_link_libraries(VehicleHalProtos
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++_unsecure        # matches libgrpc++_unsecure in proto/Android.bp
)

target_compile_options(VehicleHalProtos PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 2a  VehicleServerProtoStub
# Mirrors:  cc_library_static { name: "android.hardware.automotive.vehicle@default-grpc-libgrpc" }
# Source:   aidl/impl/4/grpc/proto/VehicleServer.proto
#
# Two --proto_path flags are required:
#   grpc/proto/   — for VehicleServer.proto itself
#   proto/        — for imported message types
#
# Note: grpc/Android.bp uses libgrpc++ (secure), not libgrpc++_unsecure.
#       VehicleServer_mock.grpc.pb.h is NOT generated (tests excluded).
# ══════════════════════════════════════════════════════════════════════════════
set(VEHICLE_SERVER_PROTO ${GRPC_SRC_DIR}/proto/VehicleServer.proto)

set(VS_PB_CC        ${GRPC_GEN_DIR}/VehicleServer.pb.cc)
set(VS_PB_H         ${GRPC_GEN_DIR}/VehicleServer.pb.h)
set(VS_GRPC_PB_CC   ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.cc)
set(VS_GRPC_PB_H    ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.h)

add_custom_command(
    OUTPUT  ${VS_PB_CC} ${VS_PB_H} ${VS_GRPC_PB_CC} ${VS_GRPC_PB_H}
    COMMAND ${PROTOC_EXE}
            --proto_path=${GRPC_SRC_DIR}/proto
            --proto_path=${PROTO_SRC_DIR}
            --cpp_out=${GRPC_GEN_DIR}
            --grpc_out=${GRPC_GEN_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${VEHICLE_SERVER_PROTO}
    DEPENDS ${VEHICLE_SERVER_PROTO} ${VEHICLE_PROTO_HDRS}
    COMMENT "Compiling gRPC service proto: VehicleServer.proto"
)

add_library(VehicleServerProtoStub STATIC
    ${VS_PB_CC}
    ${VS_GRPC_PB_CC}
)

target_include_directories(VehicleServerProtoStub
    PUBLIC  ${GRPC_GEN_DIR}
)

target_link_libraries(VehicleServerProtoStub
    PUBLIC
        VehicleHalProtos             # whole_static equivalent
        protobuf::libprotobuf
        gRPC::grpc++                 # secure grpc++ — matches grpc/Android.bp
)

target_compile_options(VehicleServerProtoStub PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 3   VehicleHalProtoMessageConverter
# Mirrors:  cc_library { name: "VehicleHalProtoMessageConverter" }
# Source:   aidl/impl/4/grpc/utils/proto_message_converter/
# ══════════════════════════════════════════════════════════════════════════════
set(PMC_DIR ${GRPC_SRC_DIR}/utils/proto_message_converter)

file(GLOB PMC_SRCS "${PMC_DIR}/src/*.cpp")

add_library(VehicleHalProtoMessageConverter STATIC ${PMC_SRCS})

target_include_directories(VehicleHalProtoMessageConverter
    PUBLIC  ${PMC_DIR}/include
)

target_link_libraries(VehicleHalProtoMessageConverter
    PUBLIC
        VehicleHalProtos
        VehicleHalUtils
        protobuf::libprotobuf
)

target_compile_options(VehicleHalProtoMessageConverter PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 4a  GRPCVehicleHardware
# Mirrors:  cc_library_static { name: "android.hardware.automotive.vehicle@default-grpc-hardware-lib" }
# Source:   aidl/impl/4/grpc/GRPCVehicleHardware.cpp
# ══════════════════════════════════════════════════════════════════════════════
add_library(GRPCVehicleHardware STATIC
    ${GRPC_SRC_DIR}/GRPCVehicleHardware.cpp
)

target_include_directories(GRPCVehicleHardware
    PUBLIC  ${GRPC_SRC_DIR}
)

target_link_libraries(GRPCVehicleHardware
    PUBLIC
        VehicleServerProtoStub
        VehicleHalProtoMessageConverter
        IVehicleHardware
        protobuf::libprotobuf
        gRPC::grpc++
)

target_compile_options(GRPCVehicleHardware PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 4b  GRPCVehicleProxyServer
# Mirrors:  cc_library_static { name: "android.hardware.automotive.vehicle@default-grpc-server-lib" }
# Source:   aidl/impl/4/grpc/GRPCVehicleProxyServer.cpp
# ══════════════════════════════════════════════════════════════════════════════
add_library(GRPCVehicleProxyServer STATIC
    ${GRPC_SRC_DIR}/GRPCVehicleProxyServer.cpp
)

target_include_directories(GRPCVehicleProxyServer
    PUBLIC  ${GRPC_SRC_DIR}
)

target_link_libraries(GRPCVehicleProxyServer
    PUBLIC
        VehicleServerProtoStub
        VehicleHalProtoMessageConverter
        IVehicleHardware
        protobuf::libprotobuf
        gRPC::grpc++
)

target_compile_options(GRPCVehicleProxyServer PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 6a  FakeObd2Frame
# Mirrors:  cc_library { name: "FakeObd2Frame" }
# Source:   aidl/impl/4/fake_impl/obd2frame/
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB OBD2_SRCS "${OBD2_DIR}/src/*.cpp")

add_library(FakeObd2Frame STATIC ${OBD2_SRCS})

target_include_directories(FakeObd2Frame
    PUBLIC  ${OBD2_DIR}/include
)

target_link_libraries(FakeObd2Frame
    PUBLIC  VehicleHalUtils
)

target_compile_options(FakeObd2Frame PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 6b  FakeUserHal
# Mirrors:  cc_library { name: "FakeUserHal" }
# Source:   aidl/impl/4/fake_impl/userhal/
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB USERHAL_SRCS "${USERHAL_DIR}/src/*.cpp")

add_library(FakeUserHal STATIC ${USERHAL_SRCS})

target_include_directories(FakeUserHal
    PUBLIC  ${USERHAL_DIR}/include
)

target_link_libraries(FakeUserHal
    PUBLIC  VehicleHalUtils
)

target_compile_options(FakeUserHal PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 6c  FakeVehicleHalValueGenerators
# Mirrors:  cc_library { name: "FakeVehicleHalValueGenerators" }
# Source:   aidl/impl/4/fake_impl/GeneratorHub/
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB GENHUB_SRCS "${GENHUB_DIR}/src/*.cpp")

add_library(FakeVehicleHalValueGenerators STATIC ${GENHUB_SRCS})

target_include_directories(FakeVehicleHalValueGenerators
    PUBLIC  ${GENHUB_DIR}/include
)

target_link_libraries(FakeVehicleHalValueGenerators
    PUBLIC
        VehicleHalUtils
        FakeObd2Frame
        JsonCpp::JsonCpp
)

target_compile_options(FakeVehicleHalValueGenerators PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 6d  VehicleHalJsonConfigLoader
# Mirrors:  cc_library { name: "VehicleHalJsonConfigLoader" }
# Source:   aidl/impl/4/default_config/JsonConfigLoader/
#
# Dropped: VehicleHalJsonConfigLoaderEnableTestProperties variant —
#          it pulls libbinder_headers and VhalTestVendorProperties
#          which are Android-only.
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB JSONCFG_SRCS "${JSONCFG_DIR}/src/*.cpp")

add_library(VehicleHalJsonConfigLoader STATIC ${JSONCFG_SRCS})

target_include_directories(VehicleHalJsonConfigLoader
    PUBLIC  ${JSONCFG_DIR}/include
)

target_link_libraries(VehicleHalJsonConfigLoader
    PUBLIC
        VehicleHalUtils
        IVehicleGeneratedHeaders
        JsonCpp::JsonCpp
)

target_compile_options(VehicleHalJsonConfigLoader PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# VHAL config root — override with -DVHAL_CONFIG_ROOT=/etc/vhal for release/RPi
# ──────────────────────────────────────────────────────────────────────────────
set(VHAL_CONFIG_ROOT "${CMAKE_SOURCE_DIR}/etc/vhal" CACHE STRING
    "Root directory for VHAL runtime config files (vhalconfig/, vhaloverride/, powercontroller/)")

message(STATUS "VHAL_CONFIG_ROOT: ${VHAL_CONFIG_ROOT}")

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 7   FakeVehicleHardware
# Mirrors:  cc_library { name: "FakeVehicleHardware" }
# Source:   aidl/impl/4/fake_impl/hardware/
#
# Dropped from Android.bp:
#   :VhalTestVendorProperties         — test vendor AIDL genrule, not in tree
#   wakeup_client_protos              — remote wakeup proto, outside tree
#   -DENABLE_VEHICLE_HAL_TEST_PROPERTIES — tied to test properties above
#   libbinder_headers                 — Android-only (FakeVehicleHardwareDefaults)
#
# If wakeup_client_protos is needed later, add a stub .proto and
# generate it as a separate CMake target.
# ══════════════════════════════════════════════════════════════════════════════
file(GLOB FAKEHW_SRCS "${FAKEHW_DIR}/src/*.cpp")

add_library(FakeVehicleHardware STATIC ${FAKEHW_SRCS})

target_include_directories(FakeVehicleHardware
    PUBLIC  ${FAKEHW_DIR}/include
)

target_link_libraries(FakeVehicleHardware
    PUBLIC
        IVehicleHardware
        VehicleHalJsonConfigLoader
        VehicleHalUtils
        FakeVehicleHalValueGenerators
        FakeObd2Frame
        FakeUserHal
        gRPC::grpc++
        JsonCpp::JsonCpp
        protobuf::libprotobuf
)

target_compile_definitions(FakeVehicleHardware PRIVATE
    VHAL_CONFIG_ROOT="${VHAL_CONFIG_ROOT}"
)

target_compile_options(FakeVehicleHardware PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 8a  DefaultVehicleHal
# Mirrors:  cc_library { name: "DefaultVehicleHal" }
# Source:   aidl/impl/4/vhal/src/
#           ConnectedClient.cpp, DefaultVehicleHal.cpp, SubscriptionManager.cpp
#
# Dropped:
#   :check_generated_enum_metadata_json — AIDL metadata validation genrule
#   libbinder_ndk                       — Android Binder IPC
#   android-automotive-large-parcelable-defaults
#
# Porting note:
#   ConnectedClient.cpp and DefaultVehicleHal.cpp reference AIBinder,
#   SpAIBinder and libbinder_ndk types throughout. These files will NOT
#   compile as-is. Required action before first build attempt:
#     (a) Add #ifdef __ANDROID__ guards around all Binder sections, or
#     (b) Replace Binder client tracking with gRPC session equivalents.
#   SubscriptionManager.cpp is likely more portable — verify first.
# ══════════════════════════════════════════════════════════════════════════════
add_library(DefaultVehicleHal STATIC
    ${VHAL_DIR}/src/ConnectedClient.cpp
    ${VHAL_DIR}/src/DefaultVehicleHal.cpp
    ${VHAL_DIR}/src/SubscriptionManager.cpp
)

target_include_directories(DefaultVehicleHal
    PUBLIC  ${VHAL_DIR}/include
)

target_link_libraries(DefaultVehicleHal
    PUBLIC
        VehicleHalUtils
        IVehicleHardware
        IVehicleGeneratedHeaders
)

target_compile_options(DefaultVehicleHal PRIVATE -Wno-unused-parameter)

# ══════════════════════════════════════════════════════════════════════════════
# LAYER 8b  vhal-core  (final executable)
# Mirrors:  cc_binary { name: "android.hardware.automotive.vehicle@V4-default-service" }
# Source:   aidl/impl/4/vhal/src/VehicleService.cpp
#
# Dropped:
#   libbinder_ndk                           — Android Binder
#   vintf_fragments, init_rc                — Android init system
#   android-automotive-large-parcelable-defaults
#
# Porting note:
#   VehicleService.cpp registers the VHAL with Android's service manager
#   via AServiceManager_addService() and joins the Binder thread pool.
#   Replace the entire main() body with gRPC server startup:
#
#       auto hardware = std::make_unique<FakeVehicleHardware>(...);
#       GRPCVehicleProxyServer server(std::move(hardware), "0.0.0.0:50051");
#       server.Run().Wait();
# ══════════════════════════════════════════════════════════════════════════════
add_executable(vhal-core
    ${VHAL_DIR}/src/VehicleService.cpp
)

target_link_libraries(vhal-core
    PRIVATE
        DefaultVehicleHal
        FakeVehicleHardware
        GRPCVehicleProxyServer
        GRPCVehicleHardware
        VehicleHalProtoMessageConverter
        VehicleServerProtoStub
        VehicleHalProtos
        VehicleHalUtils
        VehicleHalJsonConfigLoader
        FakeVehicleHalValueGenerators
        FakeObd2Frame
        FakeUserHal
        IVehicleHardware
        IVehicleGeneratedHeaders
        gRPC::grpc++
        protobuf::libprotobuf
        JsonCpp::JsonCpp
)

target_compile_options(vhal-core PRIVATE -Wno-unused-parameter)