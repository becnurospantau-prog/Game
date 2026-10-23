cmake_minimum_required(VERSION 3.20)
project(cg LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glfw glm)

add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(cg main.cpp)


add_executable(cg_week2 week2.cpp)
target_include_directories(cg_week2 PRIVATE external/stb)
target_link_libraries(cg_week2 PRIVATE glad glfw glm::glm)

add_executable(cg_week3 week3.cpp)
target_include_directories(cg_week3 PRIVATE external/stb)
target_link_libraries(cg_week3 PRIVATE glad glfw glm::glm)

add_executable(cg_week4 week4.cpp)
target_include_directories(cg_week4 PRIVATE external/stb)
target_link_libraries(cg_week4 PRIVATE glad glfw glm::glm)
target_include_directories(cg PRIVATE external/stb)
target_link_libraries(cg PRIVATE glad glfw glm::glm)

add_executable(cg_week5 week5.cpp)
target_include_directories(cg_week5 PRIVATE external/stb)
target_link_libraries(cg_week5 PRIVATE glad glfw glm::glm)


add_executable(cg_week6 week6.cpp)
target_include_directories(cg_week6 PRIVATE external/stb)
target_link_libraries(cg_week6 PRIVATE glad glfw glm::glm)


add_executable(cg_shooter shooter.cpp)
target_include_directories(cg_shooter PRIVATE external/stb)
target_link_libraries(cg_shooter PRIVATE glad glfw glm::glm)

add_executable(cg_breakout breakout.cpp)
target_include_directories(cg_breakout PRIVATE external/stb)
target_link_libraries(cg_breakout PRIVATE glad glfw glm::glm)

add_custom_command(TARGET cg POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:cg>/assets)
