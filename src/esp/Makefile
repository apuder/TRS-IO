
.PHONY: build
build:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" build

.PHONY: flash
flash:
	idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.trs-io-pp" flash

.PHONY: monitor
monitor:
	@echo "Quit with Ctrl-]."
	@echo "Toggle logging to file with Ctrl-T Ctrl-L."
	@echo "Help with Ctrl-T Ctrl-H."
	@echo "Now reset ESP."
	idf.py -p /dev/cu.usbserial-0001 monitor

