# Default optimization level
O ?= 2

all: pong61

-include build/rules.mk

LIBS = -lpthread

%.o: %.c $(BUILDSTAMP)
	$(call run,$(CC) $(CFLAGS) -O$(O) $(DEPCFLAGS) -o $@ -c,COMPILE,$<)

pong61: pong61.o
	$(call run,$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS),LINK $@)

clean: clean-main
clean-main:
	$(call run,rm -f pong61 *.o *~ *.bak core *.core,CLEAN)
	$(call run,rm -rf $(DEPSDIR))

distclean: clean

.PRECIOUS: %.o
.PHONY: all
.PHONY: clean clean-main clean-hook distclean check check-% prepare-check
