cmake_minimum_required(VERSION 3.10)
project(Game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(game
    src/main.cpp
    src/Shader.cpp
    src/physics.cpp
    src/Camera.cpp
    src/Tesseract.cpp
    src/Scene.cpp
    src/Renderer.cpp
)

target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(game
    OpenGL::OpenGL
    GLEW::GLEW
    glfw
    glm::glm
)
