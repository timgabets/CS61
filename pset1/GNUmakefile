TESTS = $(patsubst %.c,%,$(sort $(wildcard test[0-9][0-9][0-9].c)))
all: $(TESTS) hhtest

CC = $(shell if test -f /opt/local/bin/gcc-mp-4.7; then \
	    echo gcc-mp-4.7; else echo gcc; fi)
CFLAGS = -std=gnu99 -g -W -Wall
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP

-include build/rules.mk

%.o: %.c $(REBUILDSTAMP)
	$(CC) $(CFLAGS) $(DEPCFLAGS) -o $@ -c $<

all:
	@echo "*** Run 'make check' or 'make check-all' to check your work."

test%: test%.o m61.o
	$(CC) $(CFLAGS) -o $@ $^

hhtest: hhtest.o m61.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

check: $(TESTS) $(patsubst %,run-%,$(TESTS))
	@echo "*** All tests succeeded!"

check-all: $(TESTS)
	@good=true; for i in $(TESTS); do $(MAKE) run-$$i || good=false; done; \
	if $$good; then echo "*** All tests succeeded!"; fi; $$good

check-%:
	@any=false; good=true; j=`echo "$*" | sed s/^test//`; \
	for i in $(TESTS); do if expr "$$i" : "test0*$$j$$" >/dev/null; then \
	    any=true; $(MAKE) run-$$i || good=false; fi; done; \
	if $$any; then $$good; else echo "*** No such test" 1>&2; $$any; fi

run-:
	@echo "*** No such test" 1>&2; exit 1

run-%: %
	@test -d out || mkdir out
	@rm -f out/$<.fail
	@-sh -c "./$^ > out/$<.output 2>&1" >/dev/null 2>&1; true
	@perl compare.pl out/$<.output $<.c $<

clean: clean-main clean-hook
clean-main:
	rm -f $(TESTS) hhtest *.o *.dSYM core *.core
	rm -rf out $(DEPSDIR)

MALLOC_CHECK_=0
export MALLOC_CHECK_

.PRECIOUS: %.o
.PHONY: all clean clean-main clean-hook check check-all check-% run- run-%
