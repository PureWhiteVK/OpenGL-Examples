project(imgui CXX)

add_subdirectory(base)

add_executable(test_imgui)
target_sources(test_imgui PRIVATE "test.cpp")
target_link_libraries(test_imgui PRIVATE base::base)

add_executable(obj_loader)
target_sources(obj_loader PRIVATE "obj_loader.cpp")
target_link_libraries(obj_loader PRIVATE base::base)

add_executable(shader_toy)
target_sources(shader_toy PRIVATE "shader_toy.cpp")
target_link_libraries(shader_toy PRIVATE base::base)

install(
    TARGETS obj_loader shader_toy DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
