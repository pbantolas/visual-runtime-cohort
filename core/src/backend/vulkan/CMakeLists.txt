target_sources(engine PRIVATE
    renderer_vulkan.cpp
    vulkan_swapchain.cpp
    vulkan_utils.cpp
)

find_package(Vulkan QUIET)

if(Vulkan_FOUND)
    target_link_libraries(engine PRIVATE Vulkan::Vulkan)
else()
    find_path(ENGINE_VULKAN_INCLUDE_DIR
        NAMES vulkan/vulkan.h
        HINTS
            "$ENV{VULKAN_SDK}/include"
            /opt/homebrew/opt/vulkan-headers/include
            /usr/local/include
            /usr/include
    )
    find_library(ENGINE_VULKAN_LIBRARY
        NAMES vulkan
        HINTS
            "$ENV{VULKAN_SDK}/lib"
            /opt/homebrew/opt/vulkan-loader/lib
            /usr/local/lib
            /usr/lib
    )

    if(NOT ENGINE_VULKAN_INCLUDE_DIR OR NOT ENGINE_VULKAN_LIBRARY)
        message(FATAL_ERROR "ENGINE_BACKEND=Vulkan requires Vulkan headers and loader. Install a Vulkan SDK, or on macOS run: brew install vulkan-headers vulkan-loader molten-vk")
    endif()

    target_include_directories(engine PRIVATE "${ENGINE_VULKAN_INCLUDE_DIR}")
    target_link_libraries(engine PRIVATE "${ENGINE_VULKAN_LIBRARY}")
endif()

find_program(GLSLANG_VALIDATOR_EXECUTABLE
    NAMES glslangValidator
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        /opt/homebrew/bin
        /usr/local/bin
)

if(NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "ENGINE_BACKEND=Vulkan requires glslangValidator to compile GLSL shaders. Install a Vulkan SDK, or on macOS run: brew install glslang")
endif()

set(ENGINE_VULKAN_SHADER_DIR "${CMAKE_SOURCE_DIR}/core/shaders/vulkan")
set(ENGINE_VULKAN_SPIRV_DIR "${CMAKE_BINARY_DIR}/shaders/vulkan")
set(ENGINE_VULKAN_VERTEX_SHADER "${ENGINE_VULKAN_SHADER_DIR}/basic.vert.glsl")
set(ENGINE_VULKAN_FRAGMENT_SHADER "${ENGINE_VULKAN_SHADER_DIR}/basic.frag.glsl")
set(ENGINE_VULKAN_VERTEX_SPV "${ENGINE_VULKAN_SPIRV_DIR}/basic.vert.spv")
set(ENGINE_VULKAN_FRAGMENT_SPV "${ENGINE_VULKAN_SPIRV_DIR}/basic.frag.spv")

add_custom_command(
    OUTPUT "${ENGINE_VULKAN_VERTEX_SPV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_VULKAN_SPIRV_DIR}"
    COMMAND "${GLSLANG_VALIDATOR_EXECUTABLE}" -V "${ENGINE_VULKAN_VERTEX_SHADER}" -o "${ENGINE_VULKAN_VERTEX_SPV}"
    DEPENDS "${ENGINE_VULKAN_VERTEX_SHADER}"
    COMMENT "Compiling Vulkan vertex shader ${ENGINE_VULKAN_VERTEX_SHADER}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${ENGINE_VULKAN_FRAGMENT_SPV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_VULKAN_SPIRV_DIR}"
    COMMAND "${GLSLANG_VALIDATOR_EXECUTABLE}" -V "${ENGINE_VULKAN_FRAGMENT_SHADER}" -o "${ENGINE_VULKAN_FRAGMENT_SPV}"
    DEPENDS "${ENGINE_VULKAN_FRAGMENT_SHADER}"
    COMMENT "Compiling Vulkan fragment shader ${ENGINE_VULKAN_FRAGMENT_SHADER}"
    VERBATIM
)

add_custom_target(engine_vulkan_shaders
    DEPENDS
        "${ENGINE_VULKAN_VERTEX_SPV}"
        "${ENGINE_VULKAN_FRAGMENT_SPV}"
)
add_dependencies(engine engine_vulkan_shaders)

target_compile_definitions(engine PRIVATE
    ENGINE_VULKAN_VERTEX_SPV_PATH="${ENGINE_VULKAN_VERTEX_SPV}"
    ENGINE_VULKAN_FRAGMENT_SPV_PATH="${ENGINE_VULKAN_FRAGMENT_SPV}"
)

if(APPLE)
    set(MOLTENVK_ROOT "" CACHE PATH "Optional MoltenVK install root for macOS Vulkan smoke tests")
    find_file(ENGINE_MOLTENVK_ICD_JSON
        NAMES MoltenVK_icd.json
        HINTS
            "${MOLTENVK_ROOT}/etc/vulkan/icd.d"
            "$ENV{MOLTENVK_ROOT}/etc/vulkan/icd.d"
            /opt/homebrew/opt/molten-vk/etc/vulkan/icd.d
            /usr/local/opt/molten-vk/etc/vulkan/icd.d
    )

    if(NOT ENGINE_MOLTENVK_ICD_JSON)
        message(FATAL_ERROR "ENGINE_BACKEND=Vulkan on macOS requires MoltenVK for local smoke testing. Install it with: brew install molten-vk, or set MOLTENVK_ROOT.")
    endif()

    target_compile_definitions(engine PRIVATE
        ENGINE_MOLTENVK_ICD_JSON="${ENGINE_MOLTENVK_ICD_JSON}"
    )
endif()

target_compile_definitions(engine PRIVATE
    ENGINE_BACKEND_VULKAN=1
)
