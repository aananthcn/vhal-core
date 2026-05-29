# Copyright 2026 Aananth C N
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from conan import ConanFile
from conan.tools.files import copy


class VhalProtoConan(ConanFile):
    name         = "vhal-proto"
    version      = "1.0"
    description  = (
        "Android Vehicle HAL protobuf/gRPC proto definitions. "
        "Platform-independent text files; consumers run protoc themselves."
    )
    license      = "Apache-2.0"
    package_type = "header-library"
    no_copy_source = True

    # proto/ is the directory that sits alongside this conanfile.
    exports_sources = "proto/**"

    # No settings — .proto files are platform-independent text; the same
    # Conan package binary is consumed identically on pc and rpi builds.

    def package(self):
        copy(self, "*.proto",
             src=os.path.join(self.source_folder, "proto"),
             dst=os.path.join(self.package_folder, "include"),
             keep_path=True)

    def package_info(self):
        # CMake consumers: find_package(vhal-proto CONFIG REQUIRED)
        # Target:          vhal-proto::vhal-proto
        # Use:             get_target_property(<var> vhal-proto::vhal-proto
        #                      INTERFACE_INCLUDE_DIRECTORIES)
        #                  → path containing VehicleServer.proto and
        #                    android/hardware/automotive/vehicle/*.proto
        self.cpp_info.set_property("cmake_file_name",   "vhal-proto")
        self.cpp_info.set_property("cmake_target_name", "vhal-proto::vhal-proto")
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libdirs     = []
        self.cpp_info.bindirs     = []
        self.cpp_info.libs        = []
