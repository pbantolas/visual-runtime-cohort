target_sources(engine PRIVATE
    renderer_vulkan.cpp
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
