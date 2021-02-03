idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       REQUIRES libsmb2
                                main
                                trs-io)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u theTrsFileSystemModule")

