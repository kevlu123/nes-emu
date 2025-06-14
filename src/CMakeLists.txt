file(GLOB nes_files
    "*.h"
    "*.cpp"
)

add_executable(nes ${nes_files})

target_precompile_headers(nes
    PRIVATE
    "pch.h"
)

target_link_libraries(nes
    imgui
    spdlog
)
