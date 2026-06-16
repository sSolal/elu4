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
    src/Level2.cpp
    src/Renderer.cpp
    src/Object4D.cpp
    src/ObjectBuffer.cpp
    src/Menu.cpp
    src/LevelRegistry.cpp
    src/levels/CorridorLevel.cpp
    src/levels/DodgeballLevel.cpp
    src/levels/TurnAndFaceLevel.cpp
    src/levels/Level4ThirdPerson.cpp
    src/levels/StubLevel.cpp
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/backends/imgui_impl_opengl3.cpp
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

# Headless scene/query CLI — 4D math only, no OpenGL.
add_executable(4dscene
    src/scene_query_main.cpp
)

# Headless unit tests for the 4D math core (doctest, no OpenGL).
add_executable(tests
    tests/test_math4d.cpp
)

target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/imgui ${CMAKE_SOURCE_DIR}/src/imgui/backends)
target_include_directories(visualizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(generate_objects PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(4dg PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(4dscene PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

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

target_link_libraries(4dscene
    glm::glm
)

target_link_libraries(tests
    glm::glm
)

enable_testing()
add_test(NAME math4d COMMAND tests)
