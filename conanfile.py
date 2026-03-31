from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class VhalCoreConan(ConanFile):
    name = "vhal-core"
    version = "1.0"
    description = "Android 16 VHAL ported to Linux/QNX using gRPC transport"
    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"

    # ──────────────────────────────────────────────────────────────────────────
    # External dependencies
    #
    # grpc:      provides gRPC::grpc++, gRPC::grpc++_unsecure,
    #            gRPC::grpc_cpp_plugin, protobuf::protoc
    # protobuf:  pulled transitively by grpc but declared explicitly
    #            for find_package(protobuf) in CMakeLists.txt
    # jsoncpp:   libjsoncpp — used by FakeVehicleHardware, GeneratorHub,
    #            JsonConfigLoader
    # ──────────────────────────────────────────────────────────────────────────
    requires = (
        "grpc/1.69.0",
        "jsoncpp/1.9.5",
    )

    # ──────────────────────────────────────────────────────────────────────────
    # Build system generators
    # ──────────────────────────────────────────────────────────────────────────
    generators = "CMakeDeps", "CMakeToolchain"

def layout(self):
    self.folders.source = "."
    self.folders.build = "build/Release"
    self.folders.generators = "build/Release"


    # ──────────────────────────────────────────────────────────────────────────
    # Cross-compilation profiles
    #
    # Raspberry Pi OS (aarch64-linux-gnu):
    #   conan install . --output-folder=build --build=missing \
    #       --profile:host=profiles/rpi5-linux \
    #       --profile:build=profiles/linux-x86
    #
    # QNX SDP 8.0 (aarch64):
    #   conan install . --output-folder=build --build=missing \
    #       --profile:host=profiles/qnx-aarch64 \
    #       --profile:build=profiles/linux-x86
    #
    # Profile files live in profiles/ at the project root.
    # See profiles/README.md for setup instructions.
    # ──────────────────────────────────────────────────────────────────────────
