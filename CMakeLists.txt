cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

include(pico_sdk_import.cmake)
set(PICO_BOARD pico_w)

project(buddy3-irline-test C CXX ASM)
pico_sdk_init()

# Define the executable and include all source files directly
add_executable(buddy3-irline-test 
    buddy3-irline-test.c
    src/barcode.c
    src/ir-line-sensor.c
)

pico_set_program_name(buddy3-irline-test "buddy3-irline-test")
pico_set_program_version(buddy3-irline-test "0.1")

pico_enable_stdio_uart(buddy3-irline-test 0)
pico_enable_stdio_usb(buddy3-irline-test 1)

# Link standard libraries and all required hardware modules
target_link_libraries(buddy3-irline-test PRIVATE
    pico_stdlib
    hardware_uart
    hardware_adc
    hardware_gpio
)

# Include directories so headers in src/ can be found easily
target_include_directories(buddy3-irline-test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Enable UF2 output generation for flashing your Pico
pico_add_extra_outputs(buddy3-irline-test)