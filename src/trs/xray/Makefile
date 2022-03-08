
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


%.bin: %.asm
	z80asm -i $< -o $@

all: xray-stub.bin xray-load.bin
	cp xray-stub.bin ../../esp/main
	cp xray-load.bin ../../esp/components/trs-io

clean:
	rm -f *.bin

