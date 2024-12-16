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
build: html
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" build

.PHONY: flash
flash:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" flash

# Just the C++ code, not the spiffs partitions.
.PHONY: app-flash
app-flash:
	idf.py -D SDKCONFIG_DEFAULTS="$(SDKCONFIG_DEFAULTS)" app-flash

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

.PHONY: html
html:
	if [ -d html ]; then rm -r html; fi
	cp -r ../html .
	find html -name README -delete
	find html -name .DS_Store -delete
	if [ -d html/.idea ]; then rm -r html/.idea; fi
	find html -name "*.iml" -delete
	find html -type f -exec gzip --best "{}" ";"
	du -sh html

