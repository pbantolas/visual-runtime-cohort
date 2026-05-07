add_library(engine SHARED src/engine.cpp src/renderer.cpp)
target_include_directories(engine PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/Vendor/metal-cpp
)
target_link_libraries(engine PRIVATE
    "-framework Foundation"
    "-framework QuartzCore"
    "-framework Metal"
)
set_target_properties(engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
