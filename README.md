# OpenGL-Examples
OpenGL recap. (again?!)

# Building 

before conan install, we have modified some package from [conan-center-index](https://github.com/conan-io/conan-center-index)

so you have to clone my [forked version](https://github.com/PureWhiteVK/conan-center-index) of conan-center-index, build glbinding/3.5.0, glfw/3.4 and tinyobjloader/2.0.0-release

run the following commands to create package

```bash
cd recipes/glfw/all
conan create . --version "3.4"

cd recipes/glbinding/all
conan create . --version "3.5.0"

cd recipes/tinyobjloader/all
conan create . --version "2.0.0-release"
```