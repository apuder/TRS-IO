
OBJS = \
	crt0.rel \
	main.rel \
	inout.rel \
	browse.rel \
	wifi.rel \
	esp.rel \
	help.rel \
	about.rel

TRS_LIB = ../trs-lib/trs-lib.lib

CC = sdcc
AS = sdasz80
LD = sdldz80
IHEX2CMD = ../../ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc99 --opt-code-size -I../trs-lib
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0x5200 -l $(TRS_LIB) -l $(SDCC_PATH)/lib/z80/z80.lib

%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

%.cmd: %.ihx
	$(IHEX2CMD) $< $@

all: rsclient.cmd
	cp rsclient.cmd ../../esp/components/retrostore/

rsclient.ihx : $(OBJS) $(TRS_LIB) Makefile
	$(LD) $(LDFLAGS) -i rsclient.ihx $(OBJS)


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

