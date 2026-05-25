set(ENGINE_BACKEND "Metal" CACHE STRING "Renderer backend")
set_property(CACHE ENGINE_BACKEND PROPERTY STRINGS Metal Vulkan)

add_library(engine SHARED src/engine.cpp)

target_include_directories(engine PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/third_party/glm
)

target_include_directories(engine PRIVATE
    src
)

target_compile_definitions(engine PRIVATE
    ENGINE_BACKEND_NAME="${ENGINE_BACKEND}"
)

set_target_properties(engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(ENGINE_BACKEND STREQUAL "Metal")
    add_subdirectory(src/backend/metal)
elseif(ENGINE_BACKEND STREQUAL "Vulkan")
    add_subdirectory(src/backend/vulkan)
else()
    message(FATAL_ERROR "Unsupported ENGINE_BACKEND: ${ENGINE_BACKEND}")
endif()
