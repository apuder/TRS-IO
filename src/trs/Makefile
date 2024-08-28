
MAKE = make

DIRS = TRS-LIB rsc xfer


all:
	for i in $(DIRS); do echo "\n>>> Running $(MAKE) on [$$i] <<<"; $(MAKE) -C $$i || exit 1; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit 1; done
