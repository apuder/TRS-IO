
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


%.bin: %.asm
	z80asm -i $< -o $@

all: xray-stub.bin
	cp xray-stub.bin ../../esp/main

clean:
	rm -f *.bin

