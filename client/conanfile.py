"""Conan recipe for Client

This file defines the Conan package for the client application,
including OpenCV dependencies for face detection and tracking. Qt is provided externally.
"""

import os
from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load

required_conan_version = ">=2.0.0"


class ClientConan(ConanFile):
    name = "client"
    description = "Client with face tracking"
    topics = ("face-tracking", "opencv", "qt", "servo-control", "protobuf")
    license = "MIT"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    options = {
        "with_tests": [True, False],
        "with_cuda": [True, False],
        "with_boost_stacktrace": [True, False],
    }

    default_options = {
        "with_tests": False,
        "with_cuda": False,
        "with_boost_stacktrace": True,  # Will be auto-disabled if std::stacktrace is available
        # OpenCV options
        "opencv/*:with_ffmpeg": False,
        "opencv/*:with_jpeg": "libjpeg",
        "opencv/*:with_quirc": False,
        "opencv/*:with_webp": False,
        "opencv/*:with_tiff": False,
        "opencv/*:with_openexr": False,
        "opencv/*:dnn": True,
        # Qt handled externally (system packages on Linux; installer on Windows)
        # Protobuf options
        "protobuf/*:shared": False,
        "protobuf/*:with_zlib": True,
        # Boost options (only if with_boost_stacktrace is True)
        "boost/*:header_only": True,
        "boost/*:shared": False,
        # xkbcommon options
        "xkbcommon/*:with_wayland": True,
    }

    def export_sources(self):
        """Export source files needed for building"""
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        copy(self, "cmake/*", self.recipe_folder, self.export_sources_folder)
        copy(self, "src/*", self.recipe_folder, self.export_sources_folder)
        copy(self, "include/*", self.recipe_folder, self.export_sources_folder)
        copy(self, "qt/qml/*", self.recipe_folder, self.export_sources_folder)
        copy(self, "../proto/*", self.recipe_folder, self.export_sources_folder)
        if self.options.with_tests:
            copy(self, "tests/*", self.recipe_folder, self.export_sources_folder)

    def set_version(self):
        """Read version from CMakeLists.txt"""
        cmake_file = Path(self.recipe_folder) / "CMakeLists.txt"
        if cmake_file.exists():
            content = load(self, str(cmake_file))
            import re

            match = re.search(r"project\([^)]*VERSION\s+(\d+\.\d+\.\d+)", content)
            if match:
                self.version = match.group(1)
            else:
                self.version = "0.1.0"
        else:
            self.version = "0.1.0"

    def config_options(self):
        """Remove options that don't apply to current platform"""
        if self.settings.os == "Windows":
            # GTK not available on Windows
            self.options["opencv/*"].with_gtk = False
        if self.settings.os != "Linux":
            # CUDA typically only used on Linux for development
            del self.options.with_cuda

        # Check if std::stacktrace is available (C++23 feature)
        # If compiler supports it, we don't need boost::stacktrace
        compiler = self.settings.compiler
        compiler_version = self.settings.compiler.version

        # GCC 14+ and Clang 16+ have std::stacktrace support
        # MSVC 19.34+ (VS 2022 17.4+) has std::stacktrace support
        std_stacktrace_available = False
        if compiler == "gcc" and compiler_version >= "14":
            std_stacktrace_available = True
        elif compiler == "clang" and compiler_version >= "16":
            std_stacktrace_available = True
        elif compiler == "msvc" and compiler_version >= "194":  # VS 2022 17.4+
            std_stacktrace_available = True
        elif compiler == "apple-clang" and compiler_version >= "15":
            std_stacktrace_available = True

        if std_stacktrace_available:
            self.options.with_boost_stacktrace = False

    def configure(self):
        """Configure options based on settings"""
        if self.options.get_safe("with_cuda"):
            self.options["opencv/*"].with_cuda = True

    def layout(self):
        """Define the build layout"""
        self.folders.build = "."
        self.folders.generators = "."
        self.folders.source = "."

    def requirements(self):
        """Define all dependencies"""

        # Boost for stacktrace support (only if std::stacktrace not available)
        if self.options.with_boost_stacktrace:
            self.requires("boost/[>=1.82 <2]", transitive_headers=True)

        # OpenCV for face detection and tracking
        self.requires("opencv/[>=4.8 <5]")

        # Qt is provided externally (system/installer); not managed by Conan

        # Serial communication for ESP32
        # Note: Qt has QtSerialPort, but we can also use standalone if needed

        # Protobuf for serialization and communication protocol
        self.requires("protobuf/[>=3.21 <6]")

        # Test dependencies
        if self.options.with_tests:
            self.requires("doctest/[>=2.4 <3]")

    def validate(self):
        """Validate configuration"""
        # Check C++ standard
        if self.settings.compiler.get_safe("cppstd"):
            cppstd = str(self.settings.compiler.cppstd)
            # Remove gnu prefix if present
            cppstd_num = cppstd.replace("gnu", "")
            if cppstd_num.isdigit() and int(cppstd_num) < 23:
                raise ConanInvalidConfiguration("Client requires C++23 or higher")

    def generate(self):
        """Generate build system files"""
        # CMake toolchain
        tc = CMakeToolchain(self)

        # Pass options to CMake
        tc.variables["CLIENT_BUILD_TESTS"] = self.options.with_tests
        tc.variables["CLIENT_USE_CONAN"] = True
        tc.variables["CMAKE_CXX_STANDARD"] = 23
        tc.variables["CLIENT_USE_BOOST_STACKTRACE"] = self.options.with_boost_stacktrace

        if self.options.get_safe("with_cuda"):
            tc.variables["CLIENT_WITH_CUDA"] = True

        # Ninja generator doesn't support CMAKE_GENERATOR_PLATFORM or CMAKE_GENERATOR_TOOLSET
        # For Windows + MSVC, don't set these - the compiler and platform are determined
        # by the vcvarsall.bat environment setup (handled by build scripts)
        if self.settings.os == "Windows" and self.settings.compiler == "msvc":
            # Override the generic_system block to not set platform or toolset
            # These are only needed for Visual Studio generators
            tc.blocks["generic_system"].template = """
# Definition of system, platform and toolset
# For Ninja generator with MSVC:
#   - CMAKE_GENERATOR_PLATFORM not set (Ninja doesn't support it)
#   - CMAKE_GENERATOR_TOOLSET not set (Ninja doesn't support it)
#   - Architecture and toolset are determined by vcvarsall.bat environment

"""

        tc.generate()

        # CMake dependencies
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """Build the project"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package the built artifacts"""
        cmake = CMake(self)
        cmake.install()

        # Copy license
        copy(
            self,
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_info(self):
        """Define package information for consumers"""
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "Client")
        self.cpp_info.set_property("cmake_target_name", "app::client")

        # Executable location
        bindir = os.path.join(self.package_folder, "bin")
        self.cpp_info.bindirs = [bindir]

    def package_id(self):
        """Customize package ID computation"""
        # Test option doesn't affect binary compatibility
        del self.info.options.with_tests
