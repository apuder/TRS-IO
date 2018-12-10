
MAKE = make

DIRS = src


all:
	for i in $(DIRS); do $(MAKE) -C $$i || exit 1; done
	cp src/esp/build/partitions_two_ota.bin bin
	cp src/esp/build/retro-store.bin bin
	cp src/esp/build/bootloader/bootloader.bin bin
	
clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit 1; done
	rm bin/partitions_two_ota.bin
	rm bin/retro-store.bin
	rm bin/bootloader.bin