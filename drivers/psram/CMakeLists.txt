add_library(psram INTERFACE)

target_sources(psram INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/psram_init.c
)

target_include_directories(psram INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(psram INTERFACE
    hardware_clocks
    hardware_gpio
    pico_stdlib
)
