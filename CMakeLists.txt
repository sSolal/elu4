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

add_executable(visualizer
    src/visualizer_main.cpp
    src/Visualizer.cpp
    src/Shader.cpp
    src/Object4D.cpp
    src/Camera.cpp
    src/physics.cpp
)

add_executable(generate_objects
    src/generate_objects.cpp
    src/Object4D.cpp
)

add_executable(4dg
    src/cli_main.cpp
    src/Object4D.cpp
)

target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(visualizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(generate_objects PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(4dg PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(game
    OpenGL::OpenGL
    GLEW::GLEW
    glfw
    glm::glm
)

target_link_libraries(visualizer
    OpenGL::OpenGL
    GLEW::GLEW
    glfw
    glm::glm
)

target_link_libraries(generate_objects
    glm::glm
)

target_link_libraries(4dg
    glm::glm
)
