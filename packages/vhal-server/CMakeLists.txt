cmake_minimum_required(VERSION 3.22)
project(vhal-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# External dependencies
# ──────────────────────────────────────────────────────────────────────────────
find_package(jsoncpp REQUIRED CONFIG)
find_package(gRPC    REQUIRED CONFIG)

# ──────────────────────────────────────────────────────────────────────────────
# Upstream package dependencies (via Conan or add_subdirectory from root)
# ──────────────────────────────────────────────────────────────────────────────
if(NOT TARGET VehicleHalUtils)
    find_package(vhal-types REQUIRED CONFIG)
endif()
if(NOT TARGET GRPCVehicleProxyServer)
    find_package(vhal-ipc-grpc REQUIRED CONFIG)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Source paths
# ──────────────────────────────────────────────────────────────────────────────
set(IMPL_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4)
set(JSONCFG_DIR ${IMPL_DIR}/default_config/JsonConfigLoader)
set(OBD2_DIR    ${IMPL_DIR}/fake_impl/obd2frame)
set(USERHAL_DIR ${IMPL_DIR}/fake_impl/userhal)
set(GENHUB_DIR  ${IMPL_DIR}/fake_impl/GeneratorHub)
set(FAKEHW_DIR  ${IMPL_DIR}/fake_impl/hardware)
set(VHAL_DIR    ${IMPL_DIR}/vhal)

# ──────────────────────────────────────────────────────────────────────────────
# VHAL config root — override for deployment (e.g. -DVHAL_CONFIG_ROOT=/etc/vhal)
# ──────────────────────────────────────────────────────────────────────────────
set(VHAL_CONFIG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/etc/vhal" CACHE STRING
    "Root directory for VHAL runtime config files")
message(STATUS "VHAL_CONFIG_ROOT: ${VHAL_CONFIG_ROOT}")

# ──────────────────────────────────────────────────────────────────────────────
# FakeObd2Frame
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB OBD2_SRCS "${OBD2_DIR}/src/*.cpp")
add_library(FakeObd2Frame STATIC ${OBD2_SRCS})
target_include_directories(FakeObd2Frame PUBLIC ${OBD2_DIR}/include)
target_link_libraries(FakeObd2Frame PUBLIC VehicleHalUtils)
target_compile_options(FakeObd2Frame PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# FakeUserHal
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB USERHAL_SRCS "${USERHAL_DIR}/src/*.cpp")
add_library(FakeUserHal STATIC ${USERHAL_SRCS})
target_include_directories(FakeUserHal PUBLIC ${USERHAL_DIR}/include)
target_link_libraries(FakeUserHal PUBLIC VehicleHalUtils)
target_compile_options(FakeUserHal PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# FakeVehicleHalValueGenerators
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB GENHUB_SRCS "${GENHUB_DIR}/src/*.cpp")
add_library(FakeVehicleHalValueGenerators STATIC ${GENHUB_SRCS})
target_include_directories(FakeVehicleHalValueGenerators PUBLIC ${GENHUB_DIR}/include)
target_link_libraries(FakeVehicleHalValueGenerators PUBLIC
    VehicleHalUtils
    FakeObd2Frame
    JsonCpp::JsonCpp
)
target_compile_options(FakeVehicleHalValueGenerators PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# VehicleHalJsonConfigLoader
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB JSONCFG_SRCS "${JSONCFG_DIR}/src/*.cpp")
add_library(VehicleHalJsonConfigLoader STATIC ${JSONCFG_SRCS})
target_include_directories(VehicleHalJsonConfigLoader PUBLIC ${JSONCFG_DIR}/include)
target_link_libraries(VehicleHalJsonConfigLoader PUBLIC
    VehicleHalUtils
    IVehicleGeneratedHeaders
    JsonCpp::JsonCpp
)
target_compile_options(VehicleHalJsonConfigLoader PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# FakeVehicleHardware
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB FAKEHW_SRCS "${FAKEHW_DIR}/src/*.cpp")
add_library(FakeVehicleHardware STATIC ${FAKEHW_SRCS})
target_include_directories(FakeVehicleHardware PUBLIC ${FAKEHW_DIR}/include)
target_link_libraries(FakeVehicleHardware PUBLIC
    IVehicleHardware
    VehicleHalJsonConfigLoader
    VehicleHalUtils
    FakeVehicleHalValueGenerators
    FakeObd2Frame
    FakeUserHal
    gRPC::grpc++
    JsonCpp::JsonCpp
)
target_compile_definitions(FakeVehicleHardware PRIVATE
    VHAL_CONFIG_ROOT="${VHAL_CONFIG_ROOT}"
)
target_compile_options(FakeVehicleHardware PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# DefaultVehicleHal
# ──────────────────────────────────────────────────────────────────────────────
add_library(DefaultVehicleHal STATIC
    ${VHAL_DIR}/src/ConnectedClient.cpp
    ${VHAL_DIR}/src/DefaultVehicleHal.cpp
    ${VHAL_DIR}/src/SubscriptionManager.cpp
)
target_include_directories(DefaultVehicleHal PUBLIC ${VHAL_DIR}/include)
target_link_libraries(DefaultVehicleHal PUBLIC
    VehicleHalUtils
    IVehicleHardware
    IVehicleGeneratedHeaders
)
target_compile_options(DefaultVehicleHal PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# vhal-core executable
#
# Include order: GRPCVehicleProxyServer.h before FakeVehicleHardware.h —
# see CLAUDE.md "Include order guard" for the TRY_AGAIN macro collision detail.
# ──────────────────────────────────────────────────────────────────────────────
add_executable(vhal-core ${VHAL_DIR}/src/VehicleService.cpp)
target_link_libraries(vhal-core PRIVATE
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
    JsonCpp::JsonCpp
)
target_compile_options(vhal-core PRIVATE -Wno-unused-parameter)

install(TARGETS vhal-core RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/etc/
    DESTINATION etc
)
