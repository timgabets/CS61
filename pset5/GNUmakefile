# Default optimization level
O ?= 2

all: sh61

-include build/rules.mk

%.o: %.c $(BUILDSTAMP)
	$(call run,$(CC) $(CFLAGS) -O$(O) $(DEPCFLAGS) -o $@ -c,COMPILE,$<)

sh61: sh61.o
	$(call run,$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS),LINK $@)

check: sh61
	perl check.pl

check-%: sh61
	perl check.pl $(subst check-,,$@)

clean: clean-main clean-hook
clean-main:
	$(call run,rm -rf sh61 *.o *~ *.bak core *.core,CLEAN)
	$(call run,rm -rf $(DEPSDIR) out)

realclean: clean
	@echo + realclean
	$(V)rm -rf $(DISTDIR) $(DISTDIR).tar.gz

grade: grade-lab1a grade-lab1b

grade-lab1a: cmdline always
	/usr/bin/perl -w ./lab1a-tester.pl

grade-lab1b: ospsh always
	/usr/bin/perl -w ./lab1b-tester.pl

.PRECIOUS: %.o
.PHONY: all 
.PHONY: tarball clean clean-main clean-hook check check-%
