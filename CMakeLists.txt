# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Tell the SDK you're using a Pico W (change to "pico" for a regular Pico)
set(PICO_BOARD pico_w)

# Must come BEFORE project()
include(pico_sdk_import.cmake)

project(pico_gy511 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Define your executable target and source files
add_executable(pico_gy511
    imu.c
)

# Link the core standard library and the hardware I2C driver
target_link_libraries(pico_gy511
    pico_stdlib
    hardware_i2c
)

# Enable standard output over USB (so printf debugging works via Serial terminal)
pico_enable_stdio_usb(pico_gy511 1)
pico_enable_stdio_uart(pico_gy511 0)

# Create map, bin, hex, and UF2 files for flashing to the Pico W
pico_add_extra_outputs(pico_gy511)