from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class VhalGatewayConan(ConanFile):
    name = "vhal-gateway"
    version = "1.0"
    description = (
        "VHAL property gateway: subscribes to the local vhal-core via gRPC "
        "and forwards on-change property values to one or more remote nodes. "
        "Each remote node is configured with an IP address and a list of "
        "property IDs to watch. One worker thread is spawned per remote node."
    )
    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "application"

    requires = (
        "vhal-types/1.0",
        "vhal-ipc-grpc/1.0",
        "jsoncpp/1.9.5",
        "grpc/1.69.0",
    )

    exports_sources = (
        "CMakeLists.txt",
        "src/**",
        "etc/**",
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
