add_executable(${ProjectName}
        main.cpp
        lansk/critical_section.cpp
        lansk/Fmutex.cpp
        lansk/Fmutex.h
        lansk/blinker.cpp
        lansk/blinker.h

        constants.hpp

        utilities/rt_statistics.cpp
        utilities/rt_statistics.h
        utilities/crc16.cpp
        utilities/crc16.h
        utilities/intnet.h
        utilities/intnet.cpp

        shell/shell.cpp
        shell/shell.hpp

        ipstack/IPStack.cpp
        ipstack/IPStack.h
        ipstack/lwipopts.h
        ipstack/tls_common.c
        ipstack/picow_tls_client.c
        ipstack/lwipopts_tls.h

        i2c/PicoI2C.cpp
        i2c/PicoI2C.h
        i2c/eeprom.hpp
        i2c/eeprom.cpp

        uart/PicoOsUart.cpp
        uart/PicoOsUart.h

        modbus/nanomodbus.h
        modbus/nanomodbus.c
        modbus/ModbusRegister.cpp
        modbus/ModbusRegister.h
        modbus/ModbusClient.cpp
        modbus/ModbusClient.h
        modbus/sensors.hpp
        modbus/sensors.cpp

        network/NetworkSettings.h
        network/ThingSpeakPacket.h
        network/ThingSpeakClient.cpp
        network/ThingSpeakClient.h

        display/framebuf.cpp
        display/framebuf.h
        display/mono_vlsb.cpp
        display/mono_vlsb.h
#        display/ssd1306.cpp
#        display/ssd1306.h
        display/ssd1306os.h
        display/ssd1306os.cpp
        display/ui_display.hpp
        display/ui_display.cpp
        network/NetworkSettings.h
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
        modbus
        uart
        i2c
        display
        ipstack
        utilities
        lansk
        shell
        network
)

target_compile_definitions(${ProjectName} PRIVATE
        #WIFI_SSID=\"$ENV{WIFI_SSID}\"
        #WIFI_PASSWORD=\"$ENV{WIFI_PASSWORD}\"
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
        PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE=CYW43_COUNTRY_FINLAND
)

target_link_libraries(${ProjectName} 
        pico_stdlib
        hardware_i2c
        pico_time
        FreeRTOS-Kernel-Heap4
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mbedtls
        pico_mbedtls
)

# Ignore warnings from lwip code
set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
        PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-result"
)

pico_add_extra_outputs(${ProjectName})

# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
