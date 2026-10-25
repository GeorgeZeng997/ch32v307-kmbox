cmake_minimum_required(VERSION 3.16)

# Cross-compilation toolchain setup (must precede project())
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT CMAKE_C_COMPILER)
    find_program(RISCV_GCC NAMES riscv-none-elf-gcc riscv-none-embed-gcc)
    if(RISCV_GCC)
        get_filename_component(TOOLCHAIN_DIR "${RISCV_GCC}" DIRECTORY)
        get_filename_component(COMPILER_NAME "${RISCV_GCC}" NAME)
        string(REGEX REPLACE "gcc(\\.exe)?$" "" TOOLCHAIN_PREFIX "${COMPILER_NAME}")
        set(CMAKE_C_COMPILER   "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}gcc${CMAKE_EXECUTABLE_SUFFIX}")
        set(CMAKE_ASM_COMPILER "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}gcc${CMAKE_EXECUTABLE_SUFFIX}")
        set(CMAKE_OBJCOPY      "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}objcopy${CMAKE_EXECUTABLE_SUFFIX}" CACHE FILEPATH "")
        set(CMAKE_SIZE         "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}size${CMAKE_EXECUTABLE_SUFFIX}" CACHE FILEPATH "")
    endif()
endif()

project(ch32v307_kmbox C ASM)

# Detect GCC version to select rv32imac_zicsr (GCC >= 12) vs rv32imac (GCC <= 11)
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0")
    set(ARCH_FLAGS "-march=rv32imac_zicsr -mabi=ilp32")
else()
    set(ARCH_FLAGS "-march=rv32imac -mabi=ilp32")
endif()

set(COMMON_FLAGS "${ARCH_FLAGS} -msmall-data-limit=8 -mno-save-restore -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wall -Wno-unused-parameter")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS} -std=gnu99")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS} -x assembler-with-cpp")

add_compile_definitions(CH32V30x_D8C)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/User
    ${CMAKE_CURRENT_SOURCE_DIR}/User/USB_Host
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Debug
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Peripheral/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/NetLib
)

file(GLOB PERIPH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SDK/Peripheral/src/*.c")

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Startup/startup_ch32v30x_D8C.S
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Core/core_riscv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/Debug/debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/NetLib/eth_driver_10M.c
    ${PERIPH_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/User/system_ch32v30x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/ch32v30x_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/ch32v30x_usbhs_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/usbd_desc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/USB_Host/app_km.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/USB_Host/ch32v30x_usbfs_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/USB_Host/usb_host_hid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/User/USB_Host/usb_host_hub.c
)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/Ld/Link.ld"
    -nostartfiles
    -Xlinker --gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(${PROJECT_NAME}.elf
    "${CMAKE_CURRENT_SOURCE_DIR}/SDK/NetLib/libwchnet.a"
    c m nosys
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin for WCHISPTool"
)
