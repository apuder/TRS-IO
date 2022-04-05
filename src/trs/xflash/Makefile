
OBJS = \
	crt0.rel \
	xflash.rel \
	flash.rel \
	spi.rel 

CC = sdcc
AS = sdasz80
LD = sdldz80
TRS_LIB = ../TRS-LIB
IHEX2CMD = $(TRS_LIB)/ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc11 --opt-code-size -I$(TRS_LIB)/lib/include
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0x5200 -l $(TRS_LIB)/lib/trs-lib.lib -l $(SDCC_PATH)/lib/z80/z80.lib


%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

%.cmd: %.ihx
	$(IHEX2CMD) $< $@

all: xflash.cmd

xflash.ihx : $(OBJS) $(TRS_LIB) Makefile
	$(LD) $(LDFLAGS) -i xflash.ihx $(OBJS)


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

