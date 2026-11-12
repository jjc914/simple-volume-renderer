cmake_minimum_required(VERSION 3.12)

project(simple_volume_renderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Vendored dependencies -------------------------------------------------

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

# GLAD
add_library(glad STATIC vendor/glad/src/glad.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# tinyobjloader
add_library(tinyobjloader STATIC vendor/tinyobjloader/tiny_obj_loader.cc)
target_include_directories(tinyobjloader PUBLIC vendor/tinyobjloader)

# Dear ImGui
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC vendor/imgui vendor/imgui/backends)
target_link_libraries(imgui PUBLIC glfw)

# GLM (header-only) and stb_image (header-only)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE vendor/glm)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE vendor/stbimage)

# --- Application -----------------------------------------------------------

add_executable(simple_volume_renderer
    src/main.cpp
    src/application.cpp
    src/camera/orbit_camera.cpp
    src/file.cpp
    src/volume.cpp
    src/transfer_function.cpp
    src/opengl/shader.cpp
    src/renderers/grid_volume_renderer.cpp
)

target_include_directories(simple_volume_renderer PRIVATE src)
target_link_libraries(simple_volume_renderer PRIVATE
    glfw glad tinyobjloader imgui glm stb_image
)

find_package(OpenGL REQUIRED)
target_link_libraries(simple_volume_renderer PRIVATE OpenGL::GL)
if(APPLE)
    target_link_libraries(simple_volume_renderer PRIVATE
        "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# Copy resources next to the executable. ReadFile() resolves paths against the
# working directory, so run the binary from ${CMAKE_BINARY_DIR}/bin.
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources"
)
add_dependencies(simple_volume_renderer copy_resources)
