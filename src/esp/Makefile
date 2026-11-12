SHELL = /bin/bash

#
# Set the BOARD variable to one of:
#
#     trs-io-m1
#     trs-io-m3
#     trs-io-pp (default value)
#     mini-trs
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
#     update-tar-app: builds a tar file with only the ESP32 app (no HTML,
#         no FPGA files); uploads in well under a minute
#     ota-app: builds update-tar-app, uploads it over Wi-Fi, and reboots:
#
#         make ota-app IP=192.168.1.113
#
BOARD ?= trs-io-pp
PORT ?= /dev/tty.usbserial-0001

# Keep macOS tar from adding "._*" metadata entries; the firmware would write
# those to the html/fpga partitions as if they were real files.
export COPYFILE_DISABLE=1

SDKCONFIG_DEFAULTS := sdkconfig.defaults;sdkconfig.trs-io;sdkconfig.$(BOARD)
# make TRIAL=1 ...: see TRS_IO_TRIAL_BOOT in main/Kconfig
ifeq ($(TRIAL),1)
SDKCONFIG_DEFAULTS := $(SDKCONFIG_DEFAULTS);sdkconfig.trial
endif

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

# Only the ESP32 app. Everything not in the tar stays as it is on the device.
UPDATE_APP_TAR=update-app.tar
.PHONY: update-tar-app
update-tar-app: build
	rm -f $(UPDATE_APP_TAR)
	tar -c -f $(UPDATE_APP_TAR) $(TAR_OPTS) build/trs-io.bin

# Over-the-air app update through the web server's /firmware endpoint (the
# same one the "Upload Firmware" button uses). Note: every successful update
# makes the ESP reflash the FPGA on the next boot, which takes ~45 seconds.
.PHONY: ota-app
ota-app: check-ip update-tar-app
	curl --fail-with-body -sS -H "Expect:" -H "Content-Type: application/octet-stream" \
		--data-binary @$(UPDATE_APP_TAR) http://$(IP)/firmware
	curl --fail-with-body -sS -X POST -d '' http://$(IP)/reboot
	@echo "Rebooting $(IP); allow about a minute (FPGA reflash). Logs: http://$(IP)/log"

.PHONY: check-ip
check-ip:
	@if [ -z "$(IP)" ]; then echo "Set IP, e.g.: make ota-app IP=192.168.1.113"; exit 1; fi
