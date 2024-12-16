
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_definitions(-DHAVE_CONFIG_H)
endif()

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_FILES xray-load.bin
                       REQUIRES retrostore-c-sdk retrostore trs-fs xfer)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theTrsIOCoreModule")

spiffs_create_partition_image(html ../../build/html FLASH_IN_PROJECT)

if (CONFIG_TRS_IO_PP)
# spiffs_create_partition_image(elFinder ../../elFinder FLASH_IN_PROJECT)
spiffs_create_partition_image(roms ../../roms FLASH_IN_PROJECT)
spiffs_create_partition_image(fpga ../../fpga FLASH_IN_PROJECT)
endif()
