if(NOT APPLE)
    message(FATAL_ERROR "VISUAL_RUNTIME_BACKEND=Metal requires an Apple platform")
endif()

find_program(XCRUN_EXECUTABLE xcrun REQUIRED)

target_sources(visual_runtime PRIVATE
    renderer_metal.cpp
)

set(VRT_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/core/shaders/metal/basic.metal")
set(VRT_SHADER_AIR "${CMAKE_BINARY_DIR}/shaders/basic.air")
set(VRT_SHADER_LIB "${CMAKE_BINARY_DIR}/lib/basic.metallib")
set(VRT_SHADER_MODULE_CACHE "${CMAKE_BINARY_DIR}/metal-module-cache")

add_custom_command(
    OUTPUT "${VRT_SHADER_AIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${VRT_SHADER_MODULE_CACHE}"
    COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx metal -fmodules-cache-path="${VRT_SHADER_MODULE_CACHE}" -c "${VRT_SHADER_SOURCE}" -o "${VRT_SHADER_AIR}"
    DEPENDS "${VRT_SHADER_SOURCE}"
    COMMENT "Compiling Metal shader ${VRT_SHADER_SOURCE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${VRT_SHADER_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/lib"
    COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx metallib "${VRT_SHADER_AIR}" -o "${VRT_SHADER_LIB}"
    DEPENDS "${VRT_SHADER_AIR}"
    COMMENT "Linking Metal library ${VRT_SHADER_LIB}"
    VERBATIM
)

add_custom_target(visual_runtime_metal_shaders DEPENDS "${VRT_SHADER_LIB}")
add_dependencies(visual_runtime visual_runtime_metal_shaders)

target_include_directories(visual_runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/metal-cpp
)

target_compile_definitions(visual_runtime PRIVATE
    VRT_BACKEND_METAL=1
    VRT_SHADER_LIB_PATH="${VRT_SHADER_LIB}"
)

target_link_libraries(visual_runtime PRIVATE
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
)

set_target_properties(visual_runtime PROPERTIES
    LINK_DEPENDS "${VRT_SHADER_LIB}"
)
