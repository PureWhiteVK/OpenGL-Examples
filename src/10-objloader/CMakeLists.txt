project(objloader CXX)



add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/objloader.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glbinding::glbinding-aux glfw::glfw glm::glm fmt::fmt tinyobjloader::tinyobjloader)

install(TARGETS ${PROJECT_NAME} DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
