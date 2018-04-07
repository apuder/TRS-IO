
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


GEN = boot.c

%.bin: %.asm
	z80asm -i $< -o $@

%.c: %.bin
	xxd -i $< $@

all: $(GEN)

clean:
	rm -f *.bin *.c

