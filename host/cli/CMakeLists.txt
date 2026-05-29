add_executable(cli main.cpp)
add_dependencies(cli visual_runtime)
target_include_directories(cli PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/host
)
target_compile_definitions(cli PRIVATE
    VISUAL_RUNTIME_LIB_PATH="$<TARGET_FILE:visual_runtime>"
)
# Intentionally not linked against visual_runtime — loaded at runtime via dlopen
