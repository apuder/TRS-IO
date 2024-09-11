SHELL = /bin/bash

.PHONY: all
all: build pause app-flash monitor

.PHONY: build
build:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" build

.PHONY: flash
flash:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" flash

# Just the C++ code, not the spiffs partitions.
.PHONY: app-flash
app-flash:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" app-flash

.PHONY: monitor
monitor:
	@echo "Quit with Ctrl-]."
	@echo "Toggle logging to file with Ctrl-T Ctrl-L."
	@echo "Help with Ctrl-T Ctrl-H."
	@echo "Now reset ESP."
	idf.py -p /dev/cu.usbserial-0001 monitor

.PHONY: menuconfig
menuconfig:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" menuconfig

.PHONY: pause
pause:
	@read -p "Press Enter when ready to flash . . . "
