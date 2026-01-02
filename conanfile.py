from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class hello_glbindingRecipe(ConanFile):
    name = "hello-glbinding"
    version = "0.1"
    package_type = "application"

    # Optional metadata
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of hello-glbinding package here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    def requirements(self):
        self.requires("glbinding/3.5.0")
        self.requires("glfw/3.4")
        self.requires("glm/1.0.1")
        self.requires("fmt/11.2.0")
        self.requires("tinyobjloader/2.0.0-release")
        self.requires("imgui/1.92.2b-docking")
        self.requires("nativefiledialog-extended/1.2.1")
        self.requires("libuv/1.51.0")
        self.requires("sdl/2.32.10")

    def layout(self):
        self.cmake_generator = "Ninja"
        cmake_layout(self, generator=self.cmake_generator)

    def generate(self):
        # we can build dependencies here ?
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator=self.cmake_generator)
        tc.cache_variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
