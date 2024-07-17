from conan import ConanFile
from conan.tools.cmake import CMakeDeps

class ZswagRecipe(ConanFile):
    name = "zswag"
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps"

    # Specify options
    default_options = {
        "openssl*:shared": False
    }

    def requirements(self):
        self.requires("openssl/3.2.0")
        self.requires("keychain/1.3.0")
        self.requires("spdlog/1.11.0")
        self.requires("pybind11/2.10.4")
        self.requires("zlib/1.2.13")
        # keychain and libsecret have a conflict here.
        self.requires("glib/2.78.3", override=True)
