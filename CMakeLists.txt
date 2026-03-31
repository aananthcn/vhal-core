cmake_minimum_required(VERSION 3.22)
project(vhal-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ──────────────────────────────────────────────
# Conan-generated toolchain (run before this)
# ──────────────────────────────────────────────
find_package(protobuf REQUIRED CONFIG)
find_package(gRPC     REQUIRED CONFIG)

# ──────────────────────────────────────────────
# Convenience: protoc and grpc_cpp_plugin
# pulled from the Conan gRPC package
# ──────────────────────────────────────────────
get_target_property(PROTOC_EXE    protobuf::protoc   IMPORTED_LOCATION)
get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)

# ──────────────────────────────────────────────
# Paths
# ──────────────────────────────────────────────
set(PROTO_SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4/proto)
set(GRPC_SRC_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4/grpc)
set(HARDWARE_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4/hardware)
set(PROTO_GEN_DIR   ${CMAKE_BINARY_DIR}/generated/proto)
set(GRPC_GEN_DIR    ${CMAKE_BINARY_DIR}/generated/grpc)

file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
file(MAKE_DIRECTORY ${GRPC_GEN_DIR})

# ══════════════════════════════════════════════
# LAYER 1: VehicleHalProtos
# Mirrors: cc_library_static { name: "VehicleHalProtos" }
# Source:  aidl/impl/4/proto/android/hardware/automotive/vehicle/*.proto
# ══════════════════════════════════════════════

# Glob all message .proto files
file(GLOB_RECURSE VEHICLE_PROTO_FILES
    "${PROTO_SRC_DIR}/android/hardware/automotive/vehicle/*.proto"
)

# Run protoc on every message .proto file
# -I points to proto/ root so imports resolve as
#   "android/hardware/automotive/vehicle/Foo.proto"
set(VEHICLE_PROTO_SRCS "")
set(VEHICLE_PROTO_HDRS "")

foreach(PROTO_FILE ${VEHICLE_PROTO_FILES})
    # Derive output filename stem from path relative to proto/ root
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

add_library(VehicleHalProtos STATIC
    ${VEHICLE_PROTO_SRCS}
)

target_include_directories(VehicleHalProtos PUBLIC
    ${PROTO_GEN_DIR}            # so consumers include <android/hardware/.../Foo.pb.h>
)

target_link_libraries(VehicleHalProtos PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++_unsecure       # matches libgrpc++_unsecure in Android.bp
)

target_compile_options(VehicleHalProtos PRIVATE
    -Wno-unused-parameter
)

# ══════════════════════════════════════════════
# LAYER 2a: VehicleServerProtoStub
# Mirrors: cc_library_static { name: "android.hardware.automotive.vehicle@default-grpc-libgrpc" }
# Source:  aidl/impl/4/grpc/proto/VehicleServer.proto
# Note:    grpc/Android.bp switches to libgrpc++ (secure) here — not unsecure
# ══════════════════════════════════════════════

set(VEHICLE_SERVER_PROTO ${GRPC_SRC_DIR}/proto/VehicleServer.proto)

set(VS_PB_CC        ${GRPC_GEN_DIR}/VehicleServer.pb.cc)
set(VS_PB_H         ${GRPC_GEN_DIR}/VehicleServer.pb.h)
set(VS_GRPC_PB_CC   ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.cc)
set(VS_GRPC_PB_H    ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.h)
# VehicleServer_mock.grpc.pb.h is NOT generated — tests are out of scope

add_custom_command(
    OUTPUT  ${VS_PB_CC} ${VS_PB_H} ${VS_GRPC_PB_CC} ${VS_GRPC_PB_H}
    COMMAND ${PROTOC_EXE}
            --proto_path=${GRPC_SRC_DIR}/proto   # for VehicleServer.proto itself
            --proto_path=${PROTO_SRC_DIR}         # for imported message types
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

target_include_directories(VehicleServerProtoStub PUBLIC
    ${GRPC_GEN_DIR}             # VehicleServer.pb.h, VehicleServer.grpc.pb.h
)

target_link_libraries(VehicleServerProtoStub PUBLIC
    VehicleHalProtos            # whole_static equivalent — pulls in all message types
    protobuf::libprotobuf
    gRPC::grpc++                # note: secure grpc++ here, matches Android.bp
)

target_compile_options(VehicleServerProtoStub PRIVATE
    -Wno-unused-parameter
)

# ══════════════════════════════════════════════
# LAYER 2b: IVehicleHardware (header-only)
# Mirrors: header_libs: ["IVehicleHardware"]
# Source:  aidl/impl/4/hardware/include/IVehicleHardware.h
# ══════════════════════════════════════════════

add_library(IVehicleHardware INTERFACE)

target_include_directories(IVehicleHardware INTERFACE
    ${HARDWARE_DIR}/include
)

# ══════════════════════════════════════════════
# Next layers to add (subsequent Android.bp files to read):
#
#   LAYER 3: VehicleHalProtoMessageConverter
#            grpc/utils/proto_message_converter/Android.bp
#
#   LAYER 4: GRPCVehicleHardware  (default-grpc-hardware-lib)
#            GRPCVehicleProxyServer (default-grpc-server-lib)
#            both in grpc/Android.bp — already read, ready to add
#            after layer 3 is confirmed
# ══════════════════════════════════════════════