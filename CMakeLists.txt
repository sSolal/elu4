cmake_minimum_required(VERSION 3.10)
project(elu4)

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
    src/Scene.cpp
    src/Level2.cpp
    src/Renderer.cpp
    src/Minimap.cpp
    src/Object4D.cpp
    src/ObjectBuffer.cpp
    src/Menu.cpp
    src/UiTheme.cpp
    src/MenuBackground.cpp
    src/LevelRegistry.cpp
    src/levels/CorridorLevel.cpp
    src/levels/DodgeballLevel.cpp
    src/levels/TurnAndFaceLevel.cpp
    src/levels/Level4ThirdPerson.cpp
    src/levels/LeapLevel.cpp
    src/levels/PlaneLevel.cpp
    src/levels/PlaneLevel4D.cpp
    src/levels/ForestFetchLevel.cpp
    src/levels/MazeLevel.cpp
    src/levels/BigVistaLevel.cpp
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

# Optional OpenXR VR support (Route A — PCVR via WiVRn/Monado). Off by default so
# the plain desktop build needs no OpenXR/X11 dev packages. Enable with:
#   cmake -S . -B build -DBUILD_VR=ON
option(BUILD_VR "Build the game with OpenXR VR support" OFF)
if(BUILD_VR)
    find_library(OPENXR_LOADER openxr_loader REQUIRED)
    find_package(X11 REQUIRED)
    target_sources(game PRIVATE src/VR.cpp)
    target_compile_definitions(game PRIVATE
        HAVE_OPENXR
        GLFW_EXPOSE_NATIVE_X11
        GLFW_EXPOSE_NATIVE_GLX
    )
    target_link_libraries(game ${OPENXR_LOADER} OpenGL::GLX ${X11_LIBRARIES})
    message(STATUS "VR build enabled (OpenXR loader: ${OPENXR_LOADER})")
endif()

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
