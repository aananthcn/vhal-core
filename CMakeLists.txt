cmake_minimum_required(VERSION 3.22)
project(vhal-core-workspace LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────────────────────
# Developer convenience build — builds all three packages in one cmake invocation.
#
# For production use, build each package independently:
#   conan create packages/vhal-types
#   conan create packages/vhal-ipc-grpc
#   conan create packages/vhal-server
#
# Usage (dev):
#   conan install . --output-folder=build/Release --build=missing
#   cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/Release -j$(nproc)
# ──────────────────────────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(protobuf REQUIRED CONFIG)
find_package(gRPC     REQUIRED CONFIG)
find_package(jsoncpp  REQUIRED CONFIG)

# Each add_subdirectory guards its own find_package calls with
# "if(NOT TARGET ...)" so targets are defined only once.
add_subdirectory(packages/vhal-types)
add_subdirectory(packages/vhal-ipc-grpc)
add_subdirectory(packages/vhal-server)
