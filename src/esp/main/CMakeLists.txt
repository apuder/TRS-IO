set(src button.cpp led.cpp spi.cpp io.cpp main.cpp storage.cpp)
if (CONFIG_TRS_IO_ENABLE_OTA)
   set(src ${src} ota.cpp)
else()
   set(src ${src} ota-dummy.cpp)
endif()

idf_component_register(SRCS ${src}
                       INCLUDE_DIRS "include"
                       EMBED_FILES loader_frehd.bin
                       REQUIRES app_update
                                main
                                nvs_flash
                                soc
                                ulp
                                frehd
                                libsmb2
                                mdns
                                retrostore
                                tcpip
                                trs-fs
                                trs-io)

set(ulp_app_name ulp_io)
set(ulp_s_sources "ulp/io.S")
set(ulp_exp_dep_srcs "io.cpp")
ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")
