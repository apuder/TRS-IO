#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

$(call compile_only_if,$(CONFIG_TRS_IO_ENABLE_OTA),ota.o)
$(call compile_only_if_not,$(CONFIG_TRS_IO_ENABLE_OTA),ota-dummy.o)

COMPONENT_EMBED_FILES := loader_frehd.bin
