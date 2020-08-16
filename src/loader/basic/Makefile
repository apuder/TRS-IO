
CC = sdcc
AS = sdasz80
LD = sdldz80
IHEX2CMD = ../../ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc99 --opt-code-size
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0xfa00 -l $(SDCC_PATH)/lib/z80/z80.lib

OBJS = \
	crt0.rel \
	main.rel

%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

%.cmd: %.ihx
	$(IHEX2CMD) $< $@

all: loader_basic.cmd
	cp loader_basic.cmd ../../esp/components/retrostore/

loader_basic.ihx : $(OBJS) Makefile
	$(LD) $(LDFLAGS) -i loader_basic.ihx $(OBJS)


clean:
	rm -f *.rel
	rm -f *.lk
	rm -f *.lst
	rm -f *~
	rm -f *.noi
	rm -f *.ihx
	rm -f *.map
	rm -f *.asm
	rm -f *.sym
	rm -f *.cmd
