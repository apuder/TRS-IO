
OBJS = \
	crt0.rel \
	main.rel \
	browse.rel \
	load.rel \
	wifi.rel \
	esp.rel \
	help.rel \
	status.rel

TRS_LIB = ../TRS-LIB

CC = sdcc
AS = sdasz80
LD = sdldz80
IHEX2CMD = $(TRS_LIB)/ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc99 --opt-code-size -I$(TRS_LIB)/lib/include
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0x5200 -l $(TRS_LIB)/lib/trs-lib.lib -l $(SDCC_PATH)/lib/z80/z80.lib

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

