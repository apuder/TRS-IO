idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       REQUIRES app_update
                                main
                                nvs_flash
                                frehd
                                html
                                libsmb2
                                mdns
                                retrostore
                                tcpip
                                trs-fs
                                trs-io)