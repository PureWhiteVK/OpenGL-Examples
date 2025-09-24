project(imgui CXX)

# message(STATUS "imgui_resources_dir: $IMGUI")

add_executable(${PROJECT_NAME})

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
    main.cpp
    mesh.h
    mesh.cpp
    primitive.h
    primitive.cpp
    raii_helper.h
    shader.h
    shader.cpp
    window.h
    window.cpp
)


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
target_link_libraries(imgui_glfw_opengl3 PRIVATE imgui::imgui glfw::glfw)

add_library(imgui_stdlib OBJECT)
target_sources(imgui_stdlib PRIVATE 
    ${IMGUI_RES_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_RES_DIR}/misc/cpp/imgui_stdlib.h
)
target_include_directories(imgui_stdlib INTERFACE ${IMGUI_RES_DIR}/misc/cpp)
target_link_libraries(imgui_stdlib PRIVATE imgui::imgui)

# add_library(imgui_freetype OBJECT)
# target_sources(imgui_freetype PRIVATE 
#     ${IMGUI_RES_DIR}/misc/freetype/imgui_freetype.cpp
#     ${IMGUI_RES_DIR}/misc/freetype/imgui_freetype.h
# )
# target_include_directories(imgui_freetype INTERFACE ${IMGUI_RES_DIR}/misc/fretype)
# target_link_libraries(imgui_freetype PRIVATE imgui::imgui)


target_link_libraries(${PROJECT_NAME} PRIVATE glbinding::glbinding-aux glfw::glfw glm::glm fmt::fmt tinyobjloader::tinyobjloader)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui imgui_glfw_opengl3 imgui_stdlib)



install(TARGETS ${PROJECT_NAME} DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
