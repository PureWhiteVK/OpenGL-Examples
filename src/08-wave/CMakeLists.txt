project(wave CXX)



add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/wave.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE glbinding::glbinding-aux glfw::glfw glm::glm fmt::fmt)

install(TARGETS ${PROJECT_NAME} DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
