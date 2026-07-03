from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class ArraySlotThreadSafeConan(ConanFile):
    name = "arrayslotthreadsafe"
    version = "0.4.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    exports_sources = "CMakeLists.txt", "src/*"

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")
        self.test_requires("benchmark/1.8.3")

    def configure(self):
        self.settings.compiler.cppstd = "23"
        if self.settings.compiler == "clang":
            self.settings.compiler.libcxx = "libstdc++11"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # Installs the compiled .a and the raw .ixx module sources
        cmake = CMake(self)
        cmake.install()

        # Grab every .pcm produced anywhere under the build tree —
        # exact subpath varies by CMake/Ninja version, so glob instead of
        # hardcoding CMakeFiles/<target>.dir
        copy(self, "*.pcm",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "bmi"),
             keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["arrayslotthreadsafe"]

        bmi_dir = os.path.join(self.package_folder, "bmi")

        self.cpp_info.cxxflags = [
            f"-fmodule-file=Systic.System.Concurrency={bmi_dir}/Systic.System.Concurrency.pcm",
            f"-fmodule-file=Systic.System.Concurrency:CpuIntrinsics={bmi_dir}/Systic.System.Concurrency-CpuIntrinsics.pcm",
        ]

        self.cpp_info.set_property("cmake_target_name", "arrayslotthreadsafe::arrayslotthreadsafe")