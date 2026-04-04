from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class VhalCoreWorkspaceConan(ConanFile):
    """
    Root conanfile — dev-convenience only.
    Installs all external dependencies needed to build the full workspace
    (all three packages) in a single cmake invocation from the repo root.

    For production, use the per-package conanfiles instead:
        conan create packages/vhal-types
        conan create packages/vhal-ipc-grpc
        conan create packages/vhal-server
    """
    name = "vhal-core-workspace"
    version = "1.0"
    settings = "os", "arch", "compiler", "build_type"

    # Union of all package dependencies
    requires = (
        "grpc/1.69.0",
        "jsoncpp/1.9.5",
    )

    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build/Release"
        self.folders.generators = "build/Release"
