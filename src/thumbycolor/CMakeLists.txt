add_library(common_thumbycolor INTERFACE)
target_sources(common_thumbycolor INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../picosystem/i_glob.c
        ${CMAKE_CURRENT_LIST_DIR}/i_input.c
        ${CMAKE_CURRENT_LIST_DIR}/i_system.c
        ${CMAKE_CURRENT_LIST_DIR}/../picosystem/piconet.c
        ${CMAKE_CURRENT_LIST_DIR}/../picosystem/i_timer.c
        ${CMAKE_CURRENT_LIST_DIR}/i_video.c
        ${CMAKE_CURRENT_LIST_DIR}/../picosystem/stubs.c
        ${CMAKE_CURRENT_LIST_DIR}/../picosystem/i_picosound.c
)

if (PICO_ON_DEVICE)
    target_sources(common_thumbycolor INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/../picosystem/blit.S
            ${CMAKE_CURRENT_LIST_DIR}/../picosystem/picoflash.c
            )
    pico_wrap_function(common_thumbycolor malloc)
    pico_wrap_function(common_thumbycolor calloc)
    pico_wrap_function(common_thumbycolor free)
    target_compile_definitions(common_thumbycolor INTERFACE
            PICO_HEAP_SIZE=0
            USE_ZONE_FOR_MALLOC=1
            THUMBYCOLOR=1
    )
    target_link_libraries(common_thumbycolor INTERFACE hardware_i2c hardware_spi hardware_pwm hardware_irq hardware_dma)
endif()

target_include_directories(common_thumbycolor INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../textscreen
        )

target_compile_definitions(common_thumbycolor INTERFACE
        NO_USE_MOUSE=1
        PICO_AUDIO_I2S_PIO=1
        PICO_AUDIO_I2S_DMA_IRQ=1
        THUMBYCOLOR=1
        # ThumbyColor resolution: 128x80 (1.6:1 aspect ratio)
        TC_SCREENWIDTH=128
        TC_SCREENHEIGHT=80
        )

pico_generate_pio_header(common_thumbycolor ${CMAKE_CURRENT_LIST_DIR}/../picosystem/video_doom.pio)
target_link_libraries(common_thumbycolor INTERFACE pico_stdlib pico_multicore)
