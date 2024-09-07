
OBJS = \
	crt0.rel \
	main.rel

TRS_LIB = ../TRS-LIB
XFER = ../../esp/components/xfer/include

CC = sdcc
AS = sdasz80
LD = sdldz80
IHEX2CMD = $(TRS_LIB)/ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc99 --opt-code-size -I$(TRS_LIB)/lib/include -I$(XFER)
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0x5200 -l $(TRS_LIB)/lib/trs-lib.lib -l $(SDCC_PATH)/lib/z80/z80.lib

%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

%.cmd: %.ihx
	$(IHEX2CMD) $< $@

all: xfer.cmd

xfer.ihx : $(OBJS) $(TRS_LIB) Makefile
	$(LD) $(LDFLAGS) -i xfer.ihx $(OBJS)


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

