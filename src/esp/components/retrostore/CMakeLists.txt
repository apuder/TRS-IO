idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_FILES loader_cmd.bin loader_basic.cmd rsclient.cmd
                       REQUIRES trs-io retrostore-c-sdk)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theRetroStoreModule")

