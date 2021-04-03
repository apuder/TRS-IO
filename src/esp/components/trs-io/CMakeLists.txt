add_compile_definitions(MG_ENABLE_HTTP_WEBSOCKET=1)

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES index.html printer.html
                       REQUIRES retrostore trs-fs)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theTrsIOCoreModule")

