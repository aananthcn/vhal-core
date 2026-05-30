cmake_minimum_required(VERSION 3.22)
project(vhal-types LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# Source paths (all relative to this package root)
# ──────────────────────────────────────────────────────────────────────────────
set(IMPL_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/aidl/impl/4)
set(HARDWARE_DIR ${IMPL_DIR}/hardware)
set(UTILS_DIR    ${IMPL_DIR}/utils/common)
set(STUBS_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
set(GEN_HDR_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/aidl/generated_lib/4/cpp)
set(AIDL_GEN_DIR ${CMAKE_BINARY_DIR}/generated)

# ──────────────────────────────────────────────────────────────────────────────
# Generate C++ headers from .aidl files
#
# Two passes:
#   1. aidl_property/ → 117 vehicle property types
#   2. aidl/android/  → interface parcelable types (VehiclePropValue, etc.)
#
# Output: ${AIDL_GEN_DIR}/aidl/android/hardware/automotive/vehicle/*.h
# Include path exposed as: aidl/android/hardware/automotive/vehicle/Foo.h
# ──────────────────────────────────────────────────────────────────────────────
find_package(Python3 REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_aidl_headers.py
            --input  ${CMAKE_CURRENT_SOURCE_DIR}/aidl_property
            --output ${AIDL_GEN_DIR}
    RESULT_VARIABLE _aidl_result1
)
if(NOT _aidl_result1 EQUAL 0)
    message(FATAL_ERROR "vhal-types: aidl_property header generation failed")
endif()

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_aidl_headers.py
            --input  ${CMAKE_CURRENT_SOURCE_DIR}/aidl
            --output ${AIDL_GEN_DIR}
    RESULT_VARIABLE _aidl_result2
)
if(NOT _aidl_result2 EQUAL 0)
    message(FATAL_ERROR "vhal-types: aidl interface header generation failed")
endif()

# ──────────────────────────────────────────────────────────────────────────────
# AidlPropertyHeaders — interface target exposing generated AIDL headers
# ──────────────────────────────────────────────────────────────────────────────
add_library(AidlPropertyHeaders INTERFACE)
target_include_directories(AidlPropertyHeaders INTERFACE
    $<BUILD_INTERFACE:${AIDL_GEN_DIR}>
    $<INSTALL_INTERFACE:include>
)

# ──────────────────────────────────────────────────────────────────────────────
# AndroidStubs — Linux replacements for Android-specific headers
# ──────────────────────────────────────────────────────────────────────────────
add_library(AndroidStubs INTERFACE)
target_include_directories(AndroidStubs INTERFACE
    $<BUILD_INTERFACE:${STUBS_DIR}>
    $<INSTALL_INTERFACE:include>
)

# ──────────────────────────────────────────────────────────────────────────────
# IVehicleHardware — abstract backend interface (header-only)
# ──────────────────────────────────────────────────────────────────────────────
add_library(IVehicleHardware INTERFACE)
target_include_directories(IVehicleHardware INTERFACE
    $<BUILD_INTERFACE:${HARDWARE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(IVehicleHardware INTERFACE AidlPropertyHeaders)

# ──────────────────────────────────────────────────────────────────────────────
# IVehicleGeneratedHeaders — pre-generated property metadata (header-only)
#   AccessForVehicleProperty.h, ChangeModeForVehicleProperty.h, etc.
# ──────────────────────────────────────────────────────────────────────────────
add_library(IVehicleGeneratedHeaders INTERFACE)
target_include_directories(IVehicleGeneratedHeaders INTERFACE
    $<BUILD_INTERFACE:${GEN_HDR_DIR}>
    $<INSTALL_INTERFACE:include>
)

# ──────────────────────────────────────────────────────────────────────────────
# VehicleHalUtils — VehiclePropertyStore, RecurrentTimer, VehicleObjectPool, etc.
# ──────────────────────────────────────────────────────────────────────────────
file(GLOB UTILS_SRCS "${UTILS_DIR}/src/*.cpp")

add_library(VehicleHalUtils STATIC ${UTILS_SRCS})

target_include_directories(VehicleHalUtils PUBLIC
    $<BUILD_INTERFACE:${UTILS_DIR}/include>
    $<BUILD_INTERFACE:${STUBS_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(VehicleHalUtils PUBLIC
    AidlPropertyHeaders
    AndroidStubs
)
target_compile_options(VehicleHalUtils PRIVATE -Wno-unused-parameter)

# ──────────────────────────────────────────────────────────────────────────────
# Install rules (used when this package is consumed via Conan)
# ──────────────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS VehicleHalUtils AidlPropertyHeaders AndroidStubs
                IVehicleHardware IVehicleGeneratedHeaders
    EXPORT vhal-types-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install generated AIDL headers
install(DIRECTORY ${AIDL_GEN_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# Install stubs headers
install(DIRECTORY ${STUBS_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# Install VehicleHalUtils headers
install(DIRECTORY ${UTILS_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# Install IVehicleHardware headers
install(DIRECTORY ${HARDWARE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# Install pre-generated property metadata headers
install(DIRECTORY ${GEN_HDR_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT vhal-types-targets
    FILE vhal-types-targets.cmake
    NAMESPACE vhal-types::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vhal-types
)

# Generate a minimal Config.cmake so find_package(vhal-types) works when the
# package is consumed from the CMake install tree (ic-rpi superbuild) rather
# than via a Conan-generated config file.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vhal-typesConfig.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/vhal-types-targets.cmake\")\n"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vhal-typesConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vhal-types
)
