set(src button.cpp led.cpp main.cpp storage.cpp spi.cpp io.cpp)

if (CONFIG_TRS_IO_ENABLE_OTA)
   set(src ${src} ota.cpp)
else()
   set(src ${src} ota-dummy.cpp)
endif()

idf_component_register(SRCS ${src}
                       INCLUDE_DIRS "include"
                       EMBED_FILES loader_frehd.bin xray-stub.bin
                       REQUIRES app_update
                                main
                                nvs_flash
                                sdmmc
                                fatfs
                                vfs
                                frehd
                                libsmb2
                                mdns
                                retrostore
                                tcpip
                                trs-fs
                                trs-io)
