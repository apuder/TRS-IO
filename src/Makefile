
MAKE = make

DIRS = ihex2cmd loader trs


all:
	for i in $(DIRS); do echo "\n>>> Running $(MAKE) on [$$i] <<<"; $(MAKE) -C $$i || exit 1; done
	cd esp; idf.py build

.PHONY: typescript
typescript:
	cd typescript; npm run build

clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit 1; done
	cd esp; idf.py clean
