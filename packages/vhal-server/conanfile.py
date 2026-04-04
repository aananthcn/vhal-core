from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout


class VhalServerConan(ConanFile):
    name = "vhal-server"
    version = "1.0"
    description = (
        "VHAL server process: FakeVehicleHardware simulation backend, "
        "DefaultProperties.json config, and the vhal-core executable. "
        "Depends on vhal-types + vhal-ipc-grpc (swap vhal-ipc-grpc for "
        "vhal-ipc-someip to migrate transport)."
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
        "aidl/impl/4/default_config/**",
        "aidl/impl/4/fake_impl/**",
        "aidl/impl/4/vhal/**",
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
