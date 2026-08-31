from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class PortfolioHealthConan(ConanFile):
    name = "portfolio_health"
    version = "2.0.12"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"
    options = {"use_conan_libcurl": [True, False]}
    default_options = {"use_conan_libcurl": False}

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.generate()

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        if self.options.use_conan_libcurl:
            self.requires("openssl/3.2.1")
            self.requires("libcurl/8.10.1")