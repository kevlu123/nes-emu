file(GLOB nes_files
    "*.h"
    "**/*.h"
    "*.cpp"
    "**/*.cpp"
)

add_executable(nes ${nes_files})

target_include_directories(nes
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_precompile_headers(nes
    PRIVATE
    "pch.h"
)

target_link_libraries(nes
    imgui
    spdlog
)
