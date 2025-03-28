from conan import ConanFile

class AosCoreLibCpp(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("gtest/1.14.0")
        self.requires("openssl/3.2.1")

    def configure(self):
        self.options["openssl"].no_dso = False
        self.options["openssl"].shared = True
