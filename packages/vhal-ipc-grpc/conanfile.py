from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class VhalIpcGrpcConan(ConanFile):
    name = "vhal-ipc-grpc"
    version = "1.0"
    description = (
        "gRPC transport layer for VHAL: protobuf message stubs, VehicleServer "
        "gRPC service stubs, GRPCVehicleProxyServer (server adapter), and "
        "GRPCVehicleHardware (client adapter). "
        "Replace this package with vhal-ipc-someip when migrating to SOME/IP."
    )
    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "static-library"

    requires = (
        "vhal-types/1.0",
        "grpc/1.69.0",
    )

    exports_sources = (
        "CMakeLists.txt",
        "aidl/impl/4/proto/**",
        "aidl/impl/4/grpc/**",
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
        for lib in ("VehicleHalProtos", "VehicleServerProtoStub",
                    "VehicleHalProtoMessageConverter",
                    "GRPCVehicleHardware", "GRPCVehicleProxyServer"):
            self.cpp_info.components[lib].libs = [lib]
            self.cpp_info.components[lib].includedirs = ["include"]
            self.cpp_info.components[lib].requires = [
                "vhal-types::VehicleHalUtils",
                "grpc::grpc++",
            ]
