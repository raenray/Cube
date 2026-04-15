cmake_minimum_required(VERSION 3.14)
project(Cube VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ================= GLFW =================
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

# ================= GLAD =================
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/raenray/glad.git
    GIT_TAG main
)

# ================= GLM =================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)

# ================= ImGui =================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
)

FetchContent_MakeAvailable(glfw glad glm imgui)

# ================= OpenGL =================
find_package(OpenGL REQUIRED)

# ================= Executable =================
add_executable(Cube
    src/main.cpp
    src/Core/Application.cpp
    src/Core/Time.cpp
    src/Core/Input.cpp
    src/Renderer/Shader.cpp
    src/Renderer/Texture2D.cpp
    src/Renderer/Camera2D.cpp
    src/Renderer/Renderer2D.cpp

    # ImGui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

    # Game
    src/Game/ItemDatabase.cpp
    src/Game/Inventory.cpp
    src/Game/TileMap.cpp
    src/Game/Game.cpp
    src/UI/InventoryUI.cpp
)

# ================= Include =================
target_include_directories(Cube PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# ================= Link =================
target_link_libraries(Cube PRIVATE
    glfw
    glad
    glm
    OpenGL::GL
)

# ================= Define =================
target_compile_definitions(Cube PUBLIC
    ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    SAVE_DIR="${CMAKE_SOURCE_DIR}/saves"
)
