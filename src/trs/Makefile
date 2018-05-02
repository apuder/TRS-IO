
CC = sdcc
AS = sdasz80
LD = sdldz80
IHEX2CMD = ../ihex2cmd/ihex2cmd
CFLAGS = -mz80 --std-sdcc99 --opt-code-size
ASFLAGS = -l
LDFLAGS = -mjwx -b _CODE=0x5200 -l /usr/share/sdcc/lib/z80/z80.lib

OBJS = \
	crt0.rel \
	main.rel \
	key.rel \
	window.rel \
	form.rel \
	header.rel \
	list.rel \
	menu.rel \
	wifi.rel \
	panic.rel \
        font.rel

%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

%.cmd: %.ihx
	$(IHEX2CMD) $< $@

all: rsclient.cmd


rsclient.ihx : $(OBJS) Makefile
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

