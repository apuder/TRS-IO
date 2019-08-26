
%.c: %.html
	cp $< `basename $<`
	xxd -i `basename $<` $@
	rm -f `basename $<`

COMPONENT_OBJS := index.o

index.c: $(COMPONENT_PATH)/index.html

