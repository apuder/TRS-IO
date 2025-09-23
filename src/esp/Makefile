SHELL = /bin/bash

#
# Set the BOARD variable to one of:
#
#     trs-io-m1
#     trs-io-m3
#     trs-io-pp (default value)
#
# For example:
#
#     make BOARD=trs-io-m1 build
#
# Targets:
#
#     all: does a build and flashes all partitions
#     app: does a build and flashes only the app partition
#     erase-flash: erases the flash partitions
#     monitor: monitors the serial debug output
#     menuconfig: runs the ESP config program
#     build-html: packages up the HTML files
#     watch-html: watches HTML files and package them when they change
#     update-tar: builds a tar file for firmware updates
#     	  Add the SKIP_FPGA=1 command-line flag to skip the FPGA files
#
BOARD ?= trs-io-pp
PORT ?= /dev/tty.usbserial-0001

SDKCONFIG_DEFAULTS := sdkconfig.defaults;sdkconfig.trs-io;sdkconfig.$(BOARD)

.PHONY: all
all: build pause flash monitor

.PHONY: app
app: build pause app-flash monitor

.PHONY: build
build: copy-html
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" build

.PHONY: flash
flash:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -b 921600 -p $(PORT) flash

# Just the C++ code, not the spiffs partitions.
.PHONY: app-flash
app-flash:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -b 921600 -p $(PORT) app-flash

.PHONY: erase-flash
erase-flash: pause
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -b 921600 erase-flash

.PHONY: monitor
monitor:
	@echo "Quit with Ctrl-]."
	@echo "Toggle logging to file with Ctrl-T Ctrl-L."
	@echo "Help with Ctrl-T Ctrl-H."
	@echo "Now reset ESP."
	idf.py -p $(PORT) monitor

.PHONY: menuconfig
menuconfig:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" menuconfig

.PHONY: pause
pause:
	@echo "Hold down PTRS RST and press ESP RST, then hold PTRS RST another few seconds."
	@read -p "Press Enter when ready . . . "

# Run this after changing static web files. Check the resulting file
# into git.
HTML_BUILT_DIR=html/built
.PHONY: build-html
build-html:
	if [ -d $(HTML_BUILT_DIR) ]; then rm -r $(HTML_BUILT_DIR); fi
	npm run build
	cp html/favicon* $(HTML_BUILT_DIR)
	ls -lh $(HTML_BUILT_DIR)

# Run this to watch changes in static web files. Does not watch favicons.
.PHONY: watch-html
watch-html:
	if [ -d $(HTML_BUILT_DIR) ]; then rm -r $(HTML_BUILT_DIR); fi
	mkdir $(HTML_BUILT_DIR)
	cp html/favicon* $(HTML_BUILT_DIR)
	npm run watch

# Run this before every ESP build.
BUILD_DIR=build
BUILD_HTML_DIR=$(BUILD_DIR)/html
.PHONY: copy-html
copy-html:
	mkdir -p $(BUILD_DIR)
	if [ -d $(BUILD_HTML_DIR) ]; then rm -r $(BUILD_HTML_DIR); fi
	cp -r $(HTML_BUILT_DIR) build/html
	find $(BUILD_HTML_DIR) -type f -exec gzip --best "{}" ";"
	ls -lh $(BUILD_HTML_DIR)

UPDATE_TAR=update.tar
# Specify ustar format explicitly to avoid metadata files that we don't handle anyway.
TAR_OPTS=--verbose --format ustar
.PHONY: update-tar
update-tar: build
	rm -f $(UPDATE_TAR)
	cd build; tar -c -f ../$(UPDATE_TAR) $(TAR_OPTS) html/*
ifneq ($(SKIP_FPGA),1)
	tar -r -f $(UPDATE_TAR) $(TAR_OPTS) fpga/*
endif
	tar -r -f $(UPDATE_TAR) $(TAR_OPTS) build/trs-io.bin
