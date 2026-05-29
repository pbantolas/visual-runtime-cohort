find_package(glfw3 QUIET)

if(NOT glfw3_FOUND)
    message(STATUS "Skipping glfw_minimal: glfw3 was not found")
    return()
endif()

add_executable(glfw_minimal main.cpp)
add_dependencies(glfw_minimal visual_runtime)

target_include_directories(glfw_minimal PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/host
)

target_link_libraries(glfw_minimal PRIVATE glfw)

target_compile_definitions(glfw_minimal PRIVATE
    VISUAL_RUNTIME_LIB_PATH="$<TARGET_FILE:visual_runtime>"
)

if(UNIX AND NOT APPLE)
    include(CheckCXXSourceCompiles)

    option(VRT_GLFW_MINIMAL_ENABLE_XCB "Build glfw_minimal with Linux XCB native surface support" ON)
    option(VRT_GLFW_MINIMAL_ENABLE_WAYLAND "Build glfw_minimal with Linux Wayland native surface support" ON)

    find_package(X11 QUIET)
    find_path(VRT_XCB_INCLUDE_DIR NAMES xcb/xcb.h)
    find_library(VRT_XCB_LIBRARY NAMES xcb)
    find_library(VRT_X11_XCB_LIBRARY NAMES X11-xcb)
    find_path(VRT_WAYLAND_CLIENT_INCLUDE_DIR NAMES wayland-client.h)
    find_library(VRT_WAYLAND_CLIENT_LIBRARY NAMES wayland-client)

    if(VRT_GLFW_MINIMAL_ENABLE_XCB AND (NOT X11_FOUND OR NOT VRT_XCB_INCLUDE_DIR OR NOT VRT_XCB_LIBRARY OR NOT VRT_X11_XCB_LIBRARY))
        message(FATAL_ERROR "glfw_minimal on Linux requires X11, X11-xcb, and xcb development packages")
    endif()
    if(VRT_GLFW_MINIMAL_ENABLE_WAYLAND AND (NOT VRT_WAYLAND_CLIENT_INCLUDE_DIR OR NOT VRT_WAYLAND_CLIENT_LIBRARY))
        message(FATAL_ERROR "glfw_minimal on Linux requires wayland-client development packages")
    endif()

    if(VRT_GLFW_MINIMAL_ENABLE_XCB OR VRT_GLFW_MINIMAL_ENABLE_WAYLAND)
        set(CMAKE_REQUIRED_LIBRARIES glfw)
        check_cxx_source_compiles("
            #include <GLFW/glfw3.h>
            int main() { return glfwGetPlatform() == GLFW_PLATFORM_X11 ? 0 : 0; }
        " VRT_GLFW_HAS_PLATFORM_API)
        if(VRT_GLFW_MINIMAL_ENABLE_XCB)
            check_cxx_source_compiles("
                #define GLFW_EXPOSE_NATIVE_X11
                #include <GLFW/glfw3.h>
                #include <GLFW/glfw3native.h>
                int main() { return glfwGetX11Display() ? 0 : 0; }
            " VRT_GLFW_HAS_NATIVE_X11)
        endif()
        if(VRT_GLFW_MINIMAL_ENABLE_WAYLAND)
            check_cxx_source_compiles("
                #define GLFW_EXPOSE_NATIVE_WAYLAND
                #include <GLFW/glfw3.h>
                #include <GLFW/glfw3native.h>
                int main() { return glfwGetWaylandDisplay() ? 0 : 0; }
            " VRT_GLFW_HAS_NATIVE_WAYLAND)
        endif()
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif()

    if(VRT_GLFW_MINIMAL_ENABLE_XCB AND NOT VRT_GLFW_HAS_NATIVE_X11)
        message(FATAL_ERROR "glfw_minimal requires GLFW with native X11 support; disable VRT_GLFW_MINIMAL_ENABLE_XCB only for non-XCB builds")
    endif()
    if(VRT_GLFW_MINIMAL_ENABLE_WAYLAND AND NOT VRT_GLFW_HAS_NATIVE_WAYLAND)
        message(FATAL_ERROR "glfw_minimal requires GLFW with native Wayland support; disable VRT_GLFW_MINIMAL_ENABLE_WAYLAND only for XCB-only verification")
    endif()
    if(NOT VRT_GLFW_MINIMAL_ENABLE_XCB AND NOT VRT_GLFW_MINIMAL_ENABLE_WAYLAND)
        message(FATAL_ERROR "glfw_minimal requires at least one Linux native surface path")
    endif()

    if(VRT_GLFW_MINIMAL_ENABLE_XCB)
        target_compile_definitions(glfw_minimal PRIVATE VRT_GLFW_HAS_NATIVE_X11 GLFW_EXPOSE_NATIVE_X11)
        target_include_directories(glfw_minimal PRIVATE ${X11_INCLUDE_DIR})
        target_include_directories(glfw_minimal PRIVATE ${VRT_XCB_INCLUDE_DIR})
        target_link_libraries(glfw_minimal PRIVATE ${VRT_X11_XCB_LIBRARY} ${VRT_XCB_LIBRARY})
    endif()

    if(VRT_GLFW_MINIMAL_ENABLE_WAYLAND)
        target_compile_definitions(glfw_minimal PRIVATE VRT_GLFW_HAS_NATIVE_WAYLAND GLFW_EXPOSE_NATIVE_WAYLAND)
        target_include_directories(glfw_minimal PRIVATE ${VRT_WAYLAND_CLIENT_INCLUDE_DIR})
        target_link_libraries(glfw_minimal PRIVATE ${VRT_WAYLAND_CLIENT_LIBRARY})
    endif()

    if(VRT_GLFW_HAS_PLATFORM_API)
        target_compile_definitions(glfw_minimal PRIVATE VRT_GLFW_HAS_PLATFORM_API)
    endif()
else()
    message(STATUS "glfw_minimal currently implements native surface handles only for Linux/XCB and Linux/Wayland")
endif()
