
%.c: %.html
	cp $< `basename $<`
	xxd -i `basename $<` $@
	rm -f `basename $<`

COMPONENT_OBJS := index.o status.o

index.c: $(COMPONENT_PATH)/index.html

status.c: $(COMPONENT_PATH)/status.html

