set(VISUAL_RUNTIME_BACKEND "Metal" CACHE STRING "Renderer backend")
set_property(CACHE VISUAL_RUNTIME_BACKEND PROPERTY STRINGS Metal Vulkan)

add_library(visual_runtime SHARED src/visual_runtime.cpp)

target_include_directories(visual_runtime PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/third_party/glm
)

target_include_directories(visual_runtime PRIVATE
    src
)

target_compile_definitions(visual_runtime PRIVATE
    VISUAL_RUNTIME_BACKEND_NAME="${VISUAL_RUNTIME_BACKEND}"
)

set_target_properties(visual_runtime PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(VISUAL_RUNTIME_BACKEND STREQUAL "Metal")
    add_subdirectory(src/backend/metal)
elseif(VISUAL_RUNTIME_BACKEND STREQUAL "Vulkan")
    add_subdirectory(src/backend/vulkan)
else()
    message(FATAL_ERROR "Unsupported VISUAL_RUNTIME_BACKEND: ${VISUAL_RUNTIME_BACKEND}")
endif()
