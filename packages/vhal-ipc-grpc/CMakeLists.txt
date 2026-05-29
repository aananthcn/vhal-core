cmake_minimum_required(VERSION 3.22)
project(vhal-ipc-grpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# External dependencies
# ──────────────────────────────────────────────────────────────────────────────
find_package(protobuf REQUIRED CONFIG)
find_package(gRPC     REQUIRED CONFIG)

# Conan's find_program() picks up system tools (old protoc, old grpc_cpp_plugin)
# before the Conan-installed ones, causing protobuf version mismatch errors.
# Use the Conan package folder variables (set by find_package via CMakeDeps) to
# locate the exact binaries from the installed packages.
#
# For cross-compilation (e.g. building for RPi on x86_64): protobuf_PACKAGE_FOLDER_RELEASE
# points to the target-arch (armv8) package, but protoc/grpc_cpp_plugin are build tools
# that must run on the host machine. Conan deploys host-arch binaries under
# full_deploy/build/ — derive that path by replacing "host" with "build" and
# the target arch with the host arch.
if(NOT protobuf_PACKAGE_FOLDER_RELEASE)
    message(FATAL_ERROR "protobuf_PACKAGE_FOLDER_RELEASE not set — run conan install first")
endif()
if(NOT grpc_PACKAGE_FOLDER_RELEASE)
    message(FATAL_ERROR "grpc_PACKAGE_FOLDER_RELEASE not set — run conan install first")
endif()

if(CMAKE_CROSSCOMPILING)
    string(REPLACE "/host/" "/build/" _pb_tool_folder   "${protobuf_PACKAGE_FOLDER_RELEASE}")
    string(REPLACE "/host/" "/build/" _grpc_tool_folder "${grpc_PACKAGE_FOLDER_RELEASE}")
    # Replace trailing target arch with host arch (e.g. /armv8 → /x86_64)
    string(REGEX REPLACE "/[^/]+$" "/${CMAKE_HOST_SYSTEM_PROCESSOR}" _pb_tool_folder   "${_pb_tool_folder}")
    string(REGEX REPLACE "/[^/]+$" "/${CMAKE_HOST_SYSTEM_PROCESSOR}" _grpc_tool_folder "${_grpc_tool_folder}")
else()
    set(_pb_tool_folder   "${protobuf_PACKAGE_FOLDER_RELEASE}")
    set(_grpc_tool_folder "${grpc_PACKAGE_FOLDER_RELEASE}")
endif()

find_program(PROTOC_EXE NAMES protoc
    PATHS "${_pb_tool_folder}/bin" NO_DEFAULT_PATH REQUIRED)
find_program(GRPC_CPP_PLUGIN NAMES grpc_cpp_plugin
    PATHS "${_grpc_tool_folder}/bin" NO_DEFAULT_PATH REQUIRED)

message(STATUS "Using protoc:          ${PROTOC_EXE}")
message(STATUS "Using grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

# ──────────────────────────────────────────────────────────────────────────────
# vhal-types dependency (consumed via Conan find_package or add_subdirectory)
# ──────────────────────────────────────────────────────────────────────────────
if(NOT TARGET VehicleHalUtils)
    find_package(vhal-types REQUIRED CONFIG)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Source paths
# ──────────────────────────────────────────────────────────────────────────────
set(IMPL_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4)
set(PROTO_SRC_DIR ${IMPL_DIR}/proto)
set(GRPC_SRC_DIR  ${IMPL_DIR}/grpc)
set(PMC_DIR       ${GRPC_SRC_DIR}/utils/proto_message_converter)

set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/generated/proto)
set(GRPC_GEN_DIR  ${CMAKE_BINARY_DIR}/generated/grpc)

file(MAKE_DIRECTORY ${PROTO_GEN_DIR} ${GRPC_GEN_DIR})

# ──────────────────────────────────────────────────────────────────────────────
# VehicleHalProtos — compile all message .proto files
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE VEHICLE_PROTO_FILES
    "${PROTO_SRC_DIR}/android/hardware/automotive/vehicle/*.proto"
)

set(VEHICLE_PROTO_SRCS "")
set(VEHICLE_PROTO_HDRS "")

foreach(PROTO_FILE ${VEHICLE_PROTO_FILES})
    file(RELATIVE_PATH REL_PROTO ${PROTO_SRC_DIR} ${PROTO_FILE})
    string(REPLACE ".proto" "" STEM ${REL_PROTO})
    set(PB_CC ${PROTO_GEN_DIR}/${STEM}.pb.cc)
    set(PB_H  ${PROTO_GEN_DIR}/${STEM}.pb.h)
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
target_include_directories(VehicleHalProtos PUBLIC
    $<BUILD_INTERFACE:${PROTO_GEN_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(VehicleHalProtos PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++_unsecure
)
target_compile_options(VehicleHalProtos PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# VehicleServerProtoStub — gRPC service stub from VehicleServer.proto
#
# NOTE: GRPCVehicleProxyServer.h must be included before FakeVehicleHardware.h
# in any translation unit that uses both, to prevent the TRY_AGAIN macro
# (defined in <netdb.h> via grpc networking headers) from breaking the
# StatusCode enum declared in StatusCode.pb.h.
# ──────────────────────────────────────────────────────────────────────────────
set(VEHICLE_SERVER_PROTO ${GRPC_SRC_DIR}/proto/VehicleServer.proto)

set(VS_PB_CC      ${GRPC_GEN_DIR}/VehicleServer.pb.cc)
set(VS_PB_H       ${GRPC_GEN_DIR}/VehicleServer.pb.h)
set(VS_GRPC_PB_CC ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.cc)
set(VS_GRPC_PB_H  ${GRPC_GEN_DIR}/VehicleServer.grpc.pb.h)

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

add_library(VehicleServerProtoStub STATIC ${VS_PB_CC} ${VS_GRPC_PB_CC})
target_include_directories(VehicleServerProtoStub PUBLIC
    $<BUILD_INTERFACE:${GRPC_GEN_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(VehicleServerProtoStub PUBLIC
    VehicleHalProtos
    protobuf::libprotobuf
    gRPC::grpc++
)
target_compile_options(VehicleServerProtoStub PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# VehicleHalProtoMessageConverter
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB PMC_SRCS "${PMC_DIR}/src/*.cpp")

add_library(VehicleHalProtoMessageConverter STATIC ${PMC_SRCS})
target_include_directories(VehicleHalProtoMessageConverter PUBLIC
    $<BUILD_INTERFACE:${PMC_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(VehicleHalProtoMessageConverter PUBLIC
    VehicleHalProtos
    VehicleHalUtils
    protobuf::libprotobuf
)
target_compile_options(VehicleHalProtoMessageConverter PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# GRPCVehicleHardware — client-side gRPC hardware adapter
# ──────────────────────────────────────────────────────────────────────────────
add_library(GRPCVehicleHardware STATIC
    ${GRPC_SRC_DIR}/GRPCVehicleHardware.cpp
)
target_include_directories(GRPCVehicleHardware PUBLIC
    $<BUILD_INTERFACE:${GRPC_SRC_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(GRPCVehicleHardware PUBLIC
    VehicleServerProtoStub
    VehicleHalProtoMessageConverter
    IVehicleHardware
    protobuf::libprotobuf
    gRPC::grpc++
)
target_compile_options(GRPCVehicleHardware PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# GRPCVehicleProxyServer — server-side gRPC adapter
# ──────────────────────────────────────────────────────────────────────────────
add_library(GRPCVehicleProxyServer STATIC
    ${GRPC_SRC_DIR}/GRPCVehicleProxyServer.cpp
)
target_include_directories(GRPCVehicleProxyServer PUBLIC
    $<BUILD_INTERFACE:${GRPC_SRC_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(GRPCVehicleProxyServer PUBLIC
    VehicleServerProtoStub
    VehicleHalProtoMessageConverter
    IVehicleHardware
    protobuf::libprotobuf
    gRPC::grpc++
)
target_compile_options(GRPCVehicleProxyServer PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# Install rules
# ──────────────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS VehicleHalProtos VehicleServerProtoStub
                VehicleHalProtoMessageConverter
                GRPCVehicleHardware GRPCVehicleProxyServer
    EXPORT vhal-ipc-grpc-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${PROTO_GEN_DIR}/ ${GRPC_GEN_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY ${PMC_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
    ${GRPC_SRC_DIR}/GRPCVehicleProxyServer.h
    ${GRPC_SRC_DIR}/GRPCVehicleHardware.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT vhal-ipc-grpc-targets
    FILE vhal-ipc-grpc-targets.cmake
    NAMESPACE vhal-ipc-grpc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vhal-ipc-grpc
)
