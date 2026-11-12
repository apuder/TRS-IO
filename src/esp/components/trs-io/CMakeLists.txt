
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_definitions(-DHAVE_CONFIG_H)
endif()

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_FILES xray-load.bin
                       REQUIRES retrostore-c-sdk retrostore trs-fs xfer)

# The printer table is u8"" strings used as char strings, as before C++20
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-char8_t>)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theTrsIOCoreModule")

# logbuf.cpp hooks into the restart and panic paths so that the PSRAM log
# buffer is written back before reset and captures crash dumps.
set(LOGBUF_WRAPPED_FUNCTIONS esp_restart_noos panic_abort esp_panic_handler)
if(NOT CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT)
    list(APPEND LOGBUF_WRAPPED_FUNCTIONS panic_print_char panic_print_str panic_print_hex panic_print_dec)
endif()
foreach(fn ${LOGBUF_WRAPPED_FUNCTIONS})
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-Wl,--wrap=${fn}" "-u __wrap_${fn}")
endforeach()

spiffs_create_partition_image(html ../../build/html FLASH_IN_PROJECT)

if (CONFIG_TRS_IO_PP)
spiffs_create_partition_image(roms ../../roms FLASH_IN_PROJECT)
spiffs_create_partition_image(fpga ../../fpga FLASH_IN_PROJECT)
endif()
