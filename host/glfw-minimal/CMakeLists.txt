find_package(glfw3 QUIET)

if(NOT glfw3_FOUND)
    message(STATUS "Skipping glfw_minimal: glfw3 was not found")
    return()
endif()

add_executable(glfw_minimal main.cpp)
add_dependencies(glfw_minimal engine)

target_include_directories(glfw_minimal PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/host
)

target_link_libraries(glfw_minimal PRIVATE glfw)

target_compile_definitions(glfw_minimal PRIVATE
    ENGINE_LIB_PATH="$<TARGET_FILE:engine>"
)

if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    find_path(ENGINE_XCB_INCLUDE_DIR NAMES xcb/xcb.h)
    find_library(ENGINE_XCB_LIBRARY NAMES xcb)
    find_library(ENGINE_X11_XCB_LIBRARY NAMES X11-xcb)

    if(NOT X11_FOUND OR NOT ENGINE_XCB_INCLUDE_DIR OR NOT ENGINE_XCB_LIBRARY OR NOT ENGINE_X11_XCB_LIBRARY)
        message(FATAL_ERROR "glfw_minimal on Linux requires X11, X11-xcb, and xcb development packages")
    endif()

    target_compile_definitions(glfw_minimal PRIVATE GLFW_EXPOSE_NATIVE_X11)
    target_include_directories(glfw_minimal PRIVATE ${X11_INCLUDE_DIR} ${ENGINE_XCB_INCLUDE_DIR})
    target_link_libraries(glfw_minimal PRIVATE ${ENGINE_X11_XCB_LIBRARY} ${ENGINE_XCB_LIBRARY})
else()
    message(STATUS "glfw_minimal currently implements native surface handles only for Linux/XCB")
endif()
