.PHONY: all clean

all:
	$(MAKE) -C src -B
	$(MAKE) -C test -B

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	rm -f config.mk
