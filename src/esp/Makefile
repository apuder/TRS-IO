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
#
BOARD ?= trs-io-pp

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
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -b 921600 flash

# Just the C++ code, not the spiffs partitions.
.PHONY: app-flash
app-flash:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" -b 921600 app-flash

.PHONY: monitor
monitor:
	@echo "Quit with Ctrl-]."
	@echo "Toggle logging to file with Ctrl-T Ctrl-L."
	@echo "Help with Ctrl-T Ctrl-H."
	@echo "Now reset ESP."
	idf.py -p /dev/cu.usbserial-0001 monitor

.PHONY: menuconfig
menuconfig:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" menuconfig

.PHONY: pause
pause:
	@echo "Hold down PTRS RST and press ESP RST, then hold PTRS RST another few seconds."
	@read -p "Press Enter when ready to flash . . . "

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
.PHONY: dist
dist: build
	rm -rf $(UPDATE_TAR)
	cd build;tar cvf ../$(UPDATE_TAR) html/*
	tar rvf $(UPDATE_TAR) fpga/*
	tar rvf $(UPDATE_TAR) build/trs-io.bin
