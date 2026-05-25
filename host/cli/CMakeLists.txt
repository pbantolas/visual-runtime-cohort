add_executable(cli main.cpp)
add_dependencies(cli engine)
target_include_directories(cli PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/host
)
target_compile_definitions(cli PRIVATE
    ENGINE_LIB_PATH="$<TARGET_FILE:engine>"
)
# Intentionally not linked against engine — loaded at runtime via dlopen
