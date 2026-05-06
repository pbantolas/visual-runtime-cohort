add_library(engine SHARED src/engine.cpp)
target_include_directories(engine PUBLIC include)
set_target_properties(engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
