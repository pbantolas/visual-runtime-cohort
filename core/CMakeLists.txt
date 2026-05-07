set(ENGINE_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.metal")
set(ENGINE_SHADER_AIR "${CMAKE_BINARY_DIR}/shaders/basic.air")
set(ENGINE_SHADER_LIB "${CMAKE_BINARY_DIR}/lib/basic.metallib")
set(ENGINE_SHADER_MODULE_CACHE "${CMAKE_BINARY_DIR}/metal-module-cache")

add_custom_command(
    OUTPUT "${ENGINE_SHADER_AIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_SHADER_MODULE_CACHE}"
    COMMAND xcrun -sdk macosx metal -fmodules-cache-path="${ENGINE_SHADER_MODULE_CACHE}" -c "${ENGINE_SHADER_SOURCE}" -o "${ENGINE_SHADER_AIR}"
    DEPENDS "${ENGINE_SHADER_SOURCE}"
    COMMENT "Compiling Metal shader ${ENGINE_SHADER_SOURCE}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${ENGINE_SHADER_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/lib"
    COMMAND xcrun -sdk macosx metallib "${ENGINE_SHADER_AIR}" -o "${ENGINE_SHADER_LIB}"
    DEPENDS "${ENGINE_SHADER_AIR}"
    COMMENT "Linking Metal library ${ENGINE_SHADER_LIB}"
    VERBATIM
)

add_custom_target(engine_shaders DEPENDS "${ENGINE_SHADER_LIB}")

add_library(engine SHARED src/engine.cpp src/renderer.cpp)
add_dependencies(engine engine_shaders)
target_include_directories(engine PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/Vendor/metal-cpp
)
target_compile_definitions(engine PRIVATE
    ENGINE_SHADER_LIB_PATH="${ENGINE_SHADER_LIB}"
)
target_link_libraries(engine PRIVATE
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
)
set_target_properties(engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LINK_DEPENDS "${ENGINE_SHADER_LIB}"
)
