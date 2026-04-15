import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class SwaggerCppTestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"
    generators = ()

    def requirements(self):
        self.requires("cpp-httplib/0.39.0")
        self.requires(self.tested_reference_str)
        self.requires("nlohmann_json/3.12.0")
        self.requires("yaml-cpp/0.9.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        toolchain = CMakeToolchain(self)
        package_folder = self.dependencies[self.tested_reference_str].package_folder.replace("\\", "/")
        toolchain.cache_variables["swaggercpp_DIR"] = f"{package_folder}/lib/cmake/swaggercpp"
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            example = os.path.join(self.cpp.build.bindirs[0], "swaggercpp_test_package")
            self.run(example, env="conanrun")
