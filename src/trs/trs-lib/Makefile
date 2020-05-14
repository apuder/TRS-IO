
CC = sdcc
AS = sdasz80
LIB = sdcclib
CFLAGS = -mz80 --std-sdcc99 --opt-code-size
ASFLAGS = -l

OBJS = \
	screen.rel \
	window.rel \
	form.rel \
	header.rel \
	list.rel \
	menu.rel \
        font.rel \
	key.rel \
	panic.rel

%.rel: %.c
	$(CC) $(CFLAGS) -c $< 

%.rel: %.s
	$(AS) $(ASFLAGS) -o $@ $< 

all: trs-lib.lib

trs-lib.lib : $(OBJS) Makefile
	$(LIB) trs-lib.lib $(OBJS)


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
	rm -f *.lib

