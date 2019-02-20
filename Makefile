
MAKE = make

DIRS = src


all:
	for i in $(DIRS); do $(MAKE) -C $$i || exit 1; done
	mkdir -p dist
	cp src/esp/build/partitions_two_ota.bin dist
	cp src/esp/build/trs-io.bin dist
	cp src/esp/build/bootloader/bootloader.bin dist
	cp src/trs/rsclient.cmd dist
	zip -rj dist/RetroStore_gerber.zip fritzing/RetroStore_gerber
	
clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit 1; done
	rm -rf dist
