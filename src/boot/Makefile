
.SUFFIXES: .asm .bin .c

%.bin: %.asm
%.c: %.bin


GEN = boot.c

%.bin: %.asm
	z80asm -i $< -o $@

%.c: %.bin
	xxd -i $< $@

all: $(GEN)
	cp $(GEN) ../esp/components/retrostore/

clean:
	rm -f *.bin *.c

