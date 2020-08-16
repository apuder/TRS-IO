#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_ADD_LDFLAGS=-Wl,--whole-archive build/$(COMPONENT_NAME)/lib$(COMPONENT_NAME).a -Wl,--no-whole-archive

COMPONENT_EMBED_FILES := loader_basic.cmd loader_cmd.bin rsclient.cmd
