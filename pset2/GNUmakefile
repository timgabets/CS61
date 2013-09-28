TESTS = cat61 blockcat61 randomcat61 reordercat61 \
	stridecat61 ostridecat61 reverse61 pipeexchange61
STDIOTESTS = $(patsubst %,stdio-%,$(TESTS))
SLOWTESTS = $(patsubst %,slow-%,$(TESTS))

# Default optimization level
O ?= 2

all: $(TESTS) $(STDIOTESTS)
slow: $(SLOWTESTS)

-include build/rules.mk

%.o: %.c io61.h $(BUILDSTAMP)
	$(call run,$(CC) $(CFLAGS) -O$(O) $(DEPCFLAGS) -o $@ -c,COMPILE,$<)

all:
	@echo "*** Run 'make check' to check your work."

$(TESTS): %: io61.o profile61.o %.o
	$(call run,$(CC) $(CFLAGS) -o $@ $^,LINK $@)

$(SLOWTESTS): slow-%: slow-io61.o profile61.o %.o
	$(call run,$(CC) $(CFLAGS) -o $@ $^,$(shell cat $(DEPSDIR)/slow.txt))
	@echo >$(DEPSDIR)/slow.txt

$(STDIOTESTS): stdio-%: stdio-io61.o profile61.o %.o
	$(call run,$(CC) $(CFLAGS) -o $@ $^,$(shell cat $(DEPSDIR)/stdio.txt))
	@echo >$(DEPSDIR)/stdio.txt

text20meg.txt:
	echo > text20meg.txt
	while perl -e "exit((-s 'text20meg.txt') > 20000000)"; do cat /usr/share/dict/words >> text20meg.txt; done

clean: clean-main clean-hook
clean-main:
	$(call run,rm -f $(TESTS) $(SLOWTESTS) $(STDIOTESTS) *.o core *.core,CLEAN)
	$(call run,rm -rf files $(DEPSDIR))

distclean: clean
	rm -rf $(DEPSDIR)

check: $(TESTS) $(STDIOTESTS)
	perl check.pl

check-%: $(TESTS) $(STDIOTESTS)
	perl check.pl $(subst check-,,$@)

.PRECIOUS: %.o
.PHONY: all tests stdio slow \
	clean clean-main clean-hook distclean check check-% prepare-check
