cmake_minimum_required(VERSION 3.10)
project(elu4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# --- Embedded Lua 5.4 + sol2 (scripting core, vendored in src/third_party) ---
# Lua is built from the vendored amalgamated sources as a small static lib
# (the standalone lua.c/luac.c mains were dropped when vendoring). sol2 is the
# header-only C++ binding layer. Only targets that embed scripting link these;
# the math/CLI tools and the visualizer do not.
file(GLOB LUA_SRC ${CMAKE_SOURCE_DIR}/src/third_party/lua/*.c)
add_library(lua_static STATIC ${LUA_SRC})
target_include_directories(lua_static PUBLIC ${CMAKE_SOURCE_DIR}/src/third_party/lua)
set_target_properties(lua_static PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

add_library(sol2 INTERFACE)
target_include_directories(sol2 INTERFACE ${CMAKE_SOURCE_DIR}/src/third_party/sol2)
target_link_libraries(sol2 INTERFACE lua_static)
# Full runtime safety checks: type/usertype/bounds checking + clear Lua errors,
# so a buggy level script raises a catchable error instead of corrupting memory.
target_compile_definitions(sol2 INTERFACE SOL_ALL_SAFETIES_ON=1)

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
    src/Asset.cpp
    src/ObjectBuffer.cpp
    src/Menu.cpp
    src/UiTheme.cpp
    src/MenuBackground.cpp
    src/LevelRegistry.cpp
    src/levels/ScriptedLevel.cpp
    src/lua/LuaBindings.cpp
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/backends/imgui_impl_opengl3.cpp
)

# The visualizer shares the game's real rendering pipeline (Renderer + inner shaders +
# 4D occlusion) and UI theme; it just swaps the game loop for an orbit camera and a
# native file picker. No Lua/sol2 — it loads assets through the shared Asset loader.
add_executable(visualizer
    src/visualizer_main.cpp
    src/Asset.cpp
    src/Shader.cpp
    src/Object4D.cpp
    src/ObjectBuffer.cpp
    src/Level2.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/physics.cpp
    src/UiTheme.cpp
    src/third_party/tinyfiledialogs.c
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/backends/imgui_impl_glfw.cpp
    src/imgui/backends/imgui_impl_opengl3.cpp
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

# Headless smoke test for the core Lua bindings (no OpenGL).
add_executable(lua_smoke
    src/lua/lua_smoke.cpp
    src/lua/LuaBindings.cpp
)

target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/imgui ${CMAKE_SOURCE_DIR}/src/imgui/backends)
target_include_directories(visualizer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/imgui ${CMAKE_SOURCE_DIR}/src/imgui/backends)
target_include_directories(generate_objects PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(4dg PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(4dscene PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(lua_smoke PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(game
    OpenGL::OpenGL
    GLEW::GLEW
    glfw
    glm::glm
    sol2
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

target_link_libraries(lua_smoke
    glm::glm
    sol2
)

enable_testing()
add_test(NAME math4d COMMAND tests)
add_test(NAME lua_smoke COMMAND lua_smoke)
