project(base CXX)

get_target_property(IMGUI_INCLUDE_DIR imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
set(IMGUI_RES_DIR ${IMGUI_INCLUDE_DIR}/../res)

add_library(imgui_glfw_opengl3 OBJECT)
target_sources(imgui_glfw_opengl3 PRIVATE 
    ${IMGUI_RES_DIR}/bindings/imgui_impl_opengl3.h
    ${IMGUI_RES_DIR}/bindings/imgui_impl_opengl3.cpp
    ${IMGUI_RES_DIR}/bindings/imgui_impl_glfw.h
    ${IMGUI_RES_DIR}/bindings/imgui_impl_glfw.cpp
)
target_include_directories(imgui_glfw_opengl3 INTERFACE ${IMGUI_RES_DIR}/bindings)
target_link_libraries(imgui_glfw_opengl3 PUBLIC imgui::imgui glfw::glfw)
add_library(imgui::imgui_glfw_opengl3 ALIAS imgui_glfw_opengl3)

add_library(imgui_stdlib OBJECT)
target_sources(imgui_stdlib PRIVATE 
    ${IMGUI_RES_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_RES_DIR}/misc/cpp/imgui_stdlib.h
)
target_include_directories(imgui_stdlib INTERFACE ${IMGUI_RES_DIR}/misc/cpp)
target_link_libraries(imgui_stdlib PUBLIC imgui::imgui)
add_library(imgui::imgui_stdlib ALIAS imgui_stdlib)

# add_library(imgui_freetype OBJECT)
# target_sources(imgui_freetype PRIVATE 
#     ${IMGUI_RES_DIR}/misc/freetype/imgui_freetype.cpp
#     ${IMGUI_RES_DIR}/misc/freetype/imgui_freetype.h
# )
# target_include_directories(imgui_freetype INTERFACE ${IMGUI_RES_DIR}/misc/fretype)
# target_link_libraries(imgui_freetype PRIVATE imgui::imgui)

add_library(${PROJECT_NAME} STATIC)
target_sources(${PROJECT_NAME} 
    PRIVATE 
    arcball_camera.cpp
    arcball_camera.h
    camera.h
    drawable.h
    fps_camera.cpp
    fps_camera.h
    line_loops.cpp
    line_loops.h
    lines.cpp
    lines.h
    mesh.cpp
    mesh.h
    primitive.cpp
    primitive.h
    raii_helper.h
    shader.cpp
    shader.h
    window.cpp
    window.h
)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC 
    glbinding::glbinding-aux 
    glfw::glfw glm::glm 
    fmt::fmt 
    tinyobjloader::tinyobjloader
    imgui::imgui
    imgui::imgui_stdlib 
    imgui::imgui_glfw_opengl3
    nfd::nfd
    $<$<PLATFORM_ID:Windows>:libuv::uv_a>
    $<$<PLATFORM_ID:Darwin>:uv>
)

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS ${PROJECT_NAME})

install(
    TARGETS ${PROJECT_NAME} DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
