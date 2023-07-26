from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMake, CMakeToolchain, cmake_layout

class ZswagConan(ConanFile):
    name = "zswag"
    version = "master"
    license = "BSD-3"
    author = "Navigation Data Standard e.V. <support@nds-association.org>"
    url = "https://github.com/ndsev/zswag"
    description = "Zserio services over HTTP and OpenAPI"
    topics = ("zserio", "http", "openapi")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "ZSWAG_KEYCHAIN_SUPPORT": [True, False]}
    default_options = {"shared": True, "openssl*:shared": True, "ZSWAG_KEYCHAIN_SUPPORT": True}
    generators = "CMakeDeps"
    requires = "openssl/1.1.1t", "pybind11/2.10.4", "keychain/1.2.1", "spdlog/1.11.0"
    build_policy = "missing"
    exports_sources = "conanfile.py", "CMakeLists.txt", "libs/*", "cmake/*"  # Include all sources

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build(target="httpcl")
        cmake.build(target="zswagcl")

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["zswag"]
