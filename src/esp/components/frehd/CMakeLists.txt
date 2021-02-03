idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       REQUIRES trs-fs)

# Force static linking of this component
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-u init_frehd")
