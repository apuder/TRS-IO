
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_definitions(-DHAVE_CONFIG_H)
endif()

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES index.html printer.html AnotherMansTreasureMIII64C.ttf
                       EMBED_FILES xray-load.bin
                       REQUIRES retrostore trs-fs)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theTrsIOCoreModule")

