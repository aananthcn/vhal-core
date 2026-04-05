cmake_minimum_required(VERSION 3.22)
project(vhal-gateway LANGUAGES CXX)

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
if(NOT TARGET GRPCVehicleHardware)
    find_package(vhal-ipc-grpc REQUIRED CONFIG)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Gateway config root — override for deployment (e.g. -DGATEWAY_CONFIG_ROOT=...)
# ──────────────────────────────────────────────────────────────────────────────
set(GATEWAY_CONFIG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/etc/vhal" CACHE STRING
    "Root directory for gateway runtime config files")
message(STATUS "GATEWAY_CONFIG_ROOT: ${GATEWAY_CONFIG_ROOT}")

# ──────────────────────────────────────────────────────────────────────────────
# VhalGatewayLib — all logic except main()
# ──────────────────────────────────────────────────────────────────────────────
add_library(VhalGatewayLib STATIC
    src/GatewayConfig.cpp
    src/NodeForwarder.cpp
    src/VhalGateway.cpp
)
target_include_directories(VhalGatewayLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(VhalGatewayLib PUBLIC
    GRPCVehicleHardware
    VehicleServerProtoStub
    VehicleHalProtoMessageConverter
    VehicleHalUtils
    IVehicleHardware
    JsonCpp::JsonCpp
    gRPC::grpc++
)
target_compile_definitions(VhalGatewayLib PRIVATE
    GATEWAY_CONFIG_ROOT="${GATEWAY_CONFIG_ROOT}"
)
target_compile_options(VhalGatewayLib PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# vhal-gateway executable
# ──────────────────────────────────────────────────────────────────────────────
add_executable(vhal-gateway src/main.cpp)
target_link_libraries(vhal-gateway PRIVATE VhalGatewayLib)
target_compile_definitions(vhal-gateway PRIVATE
    GATEWAY_CONFIG_ROOT="${GATEWAY_CONFIG_ROOT}"
)
target_compile_options(vhal-gateway PRIVATE -Wno-unused-parameter)

install(TARGETS vhal-gateway RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/etc/
    DESTINATION etc
)
