
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


%.bin: %.asm
	z80asm -i $< -o $@

%.c: %.bin
	xxd -i $< $@

all: loader_frehd.bin
	cp loader_frehd.bin ../../esp/main

clean:
	rm -f *.bin *.c

