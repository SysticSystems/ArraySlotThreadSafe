from conan import ConanFile
from conan.tools.cmake import cmake_layout

class SysticSkeletonConan(ConanFile):
    name = "your_project_name"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("gtest/1.14.0")
        self.requires("benchmark/1.8.3")

    def configure(self):
        # Ensure everyone is using the same C++ standard
        # Validating C++23.
        self.settings.compiler.cppstd = "23"
        
        # Ensure we use the correct ABI for Clang on Linux
        if self.settings.compiler == "clang":
            self.settings.compiler.libcxx = "libstdc++11"

    def build_requirements(self):
        pass