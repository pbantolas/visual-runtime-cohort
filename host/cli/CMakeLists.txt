add_executable(cli main.cpp)
target_include_directories(cli PRIVATE
    ../../core/include   # engine/api.h
    ..                   # engine_api.h
)
target_compile_definitions(cli PRIVATE
    ENGINE_LIB_PATH="${CMAKE_BINARY_DIR}/lib/libengine.dylib"
)
# Intentionally not linked against engine — loaded at runtime via dlopen
