from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class VhalTypesConan(ConanFile):
    name = "vhal-types"
    version = "1.0"
    description = (
        "Transport-agnostic VHAL domain types: AIDL-generated vehicle property "
        "headers, IVehicleHardware interface, VehicleHalUtils. "
        "Zero transport dependency — survives gRPC → SOME/IP migration."
    )
    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "static-library"

    # No external library dependencies — only Python3 at configure time
    # (for generate_aidl_headers.py) which is a build tool, not a link dep.

    exports_sources = (
        "CMakeLists.txt",
        "aidl_property/**",
        "aidl/android/**",
        "aidl/generated_lib/**",
        "aidl/impl/4/hardware/**",
        "aidl/impl/4/utils/**",
        "stubs/**",
        "scripts/**",
    )

    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        inc = os.path.join(self.package_folder, "include")
        lib = os.path.join(self.package_folder, "lib")

        for comp in ("AidlPropertyHeaders", "AndroidStubs",
                     "IVehicleGeneratedHeaders", "IVehicleHardware"):
            self.cpp_info.components[comp].includedirs = [inc]
            self.cpp_info.components[comp].libdirs = []
            self.cpp_info.components[comp].libs = []

        self.cpp_info.components["VehicleHalUtils"].libs = ["VehicleHalUtils"]
        self.cpp_info.components["VehicleHalUtils"].includedirs = [inc]
        self.cpp_info.components["VehicleHalUtils"].libdirs = [lib]
        self.cpp_info.components["VehicleHalUtils"].requires = [
            "AidlPropertyHeaders",
            "AndroidStubs",
        ]
