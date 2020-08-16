
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


%.bin: %.asm
	z80asm -i $< -o $@

%.c: %.bin
	xxd -i $< $@

all: loader_cmd.c
	cp loader_cmd.bin ../../esp/components/retrostore/

clean:
	rm -f *.bin *.c

