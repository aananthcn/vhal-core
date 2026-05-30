import os
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
        inc = os.path.join(self.package_folder, "include")
        libdir = os.path.join(self.package_folder, "lib")

        def _comp(name, requires):
            self.cpp_info.components[name].libs = [name]
            self.cpp_info.components[name].includedirs = [inc]
            self.cpp_info.components[name].libdirs = [libdir]
            self.cpp_info.components[name].requires = requires

        # Mirror the target_link_libraries() graph in vhal-ipc-grpc/CMakeLists.txt.
        # Same-package components are referenced by bare name; cross-package by pkg::comp.
        # protobuf is a transitive dep via grpc — reference grpc::grpc++ only.
        _comp("VehicleHalProtos", [
            "grpc::grpc++",
        ])
        _comp("VehicleServerProtoStub", [
            "VehicleHalProtos",
            "grpc::grpc++",
        ])
        _comp("VehicleHalProtoMessageConverter", [
            "VehicleHalProtos",
            "vhal-types::VehicleHalUtils",
            "grpc::grpc++",
        ])
        _comp("GRPCVehicleHardware", [
            "VehicleServerProtoStub",
            "VehicleHalProtoMessageConverter",
            "vhal-types::IVehicleHardware",
            "grpc::grpc++",
        ])
        _comp("GRPCVehicleProxyServer", [
            "VehicleServerProtoStub",
            "VehicleHalProtoMessageConverter",
            "vhal-types::IVehicleHardware",
            "grpc::grpc++",
        ])
