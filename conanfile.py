from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class SwaggerCppRecipe(ConanFile):
    name = "swaggercpp"
    version = "0.2.0"
    package_type = "library"
    license = "MIT"
    author = "Sergio Triana Escobedo"
    url = "https://github.com/your-org/swagger-cpp"
    description = "Professional C++23 library for Swagger/OpenAPI parsing, validation, traversal, and serialization."
    topics = ("swagger", "openapi", "openapi3", "cpp23", "api")
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True, "yaml-cpp/*:shared": True}
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "LICENSE",
    )
    test_package_folder = "packaging/conan/test_package"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        self.requires("cpp-httplib/[>=0.18 <1]")
        self.requires("nlohmann_json/[>=3.12 <4]")
        self.requires("yaml-cpp/[>=0.8 <1]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.variables["SWAGGERCPP_BUILD_TESTS"] = False
        toolchain.variables["SWAGGERCPP_BUILD_BENCHMARKS"] = False
        toolchain.variables["SWAGGERCPP_BUILD_EXAMPLES"] = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=self.package_folder)

    def package_info(self):
        self.cpp_info.libs = ["swaggercpp"]
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.set_property("cmake_file_name", "swaggercpp")
        self.cpp_info.set_property("cmake_target_name", "swaggercpp::swaggercpp")
        self.cpp_info.includedirs = ["include"]
