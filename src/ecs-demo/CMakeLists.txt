cmake_minimum_required(VERSION 3.14)
project(nexus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(nexus)

function(make_posix_path input output)
    file(TO_CMAKE_PATH "${input}" tmp)
		# remove last /
    string(REGEX REPLACE "/$" "" tmp "${tmp}")
    set(${output} "${tmp}" PARENT_SCOPE)
endfunction()

make_posix_path("${CMAKE_CURRENT_SOURCE_DIR}" SOURCE_DIR)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
	SOURCE_DIR="${SOURCE_DIR}"
)

target_compile_options(
	nexus
	PRIVATE
	-fno-exceptions
	-Wall)

target_sources(
	nexus
	PRIVATE
	Source/Graphics/Shader.cpp
	Source/Main.cpp
	Source/Systems/CameraControlSystem.cpp
	Source/Systems/PhysicsSystem.cpp
	Source/Systems/PlayerControlSystem.cpp
	Source/Systems/RenderSystem.cpp
	Source/WindowManager.cpp)

target_sources(
	nexus
	PRIVATE
	Source/Components/Camera.hpp
	Source/Components/Gravity.hpp
	Source/Components/Player.hpp
	Source/Components/Renderable.hpp
	Source/Components/RigidBody.hpp
	Source/Components/Thrust.hpp
	Source/Components/Transform.hpp
	Source/Core/ComponentArray.hpp
	Source/Core/ComponentManager.hpp
	Source/Core/Coordinator.hpp
	Source/Core/EntityManager.hpp
	Source/Core/EventManager.hpp
	Source/Core/System.hpp
	Source/Core/SystemManager.hpp
	Source/Core/Types.hpp
	Source/Math/Vec2.hpp
	Source/Math/Vec3.hpp
	Source/Math/Vec4.hpp)

target_include_directories(
	nexus
	PRIVATE
	Source)

target_link_libraries(
	nexus
	PRIVATE
	glbinding::glbinding-aux glfw::glfw
)

