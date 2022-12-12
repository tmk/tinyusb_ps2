cmake_minimum_required(VERSION 3.5)

#include(${CMAKE_CURRENT_SOURCE_DIR}/../../../hw/bsp/family_support.cmake)
include(${TINYUSB_PATH}/hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT})

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ps2.c
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
        )

# Example include
target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

# Configure compilation flags and libraries for the example... see the corresponding function
# in hw/bsp/FAMILY/family.cmake for details.
family_configure_device_example(${PROJECT})

# Enable stdio_usb for printf on CDC
pico_enable_stdio_usb(${PROJECT} 1)


# pico-sdk/src/rp2_common/hardware_flash/flash.c
#   error: declaration of 'flash_range_erase' shadows a global declaration
target_compile_options(${PROJECT} PUBLIC
        #-Wno-error=shadow
        -Wno-shadow
        )
