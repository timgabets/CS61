OBJDIR	:= obj
DEPSDIR	:= $(OBJDIR)/.deps
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
REBUILD =
comma	= ,

# Cross-compiler toolchain
CC	= $(GCCPREFIX)gcc
CXX	= $(GCCPREFIX)c++
AS	= $(GCCPREFIX)as
AR	= $(GCCPREFIX)ar
LD	= $(GCCPREFIX)ld
OBJCOPY	= $(GCCPREFIX)objcopy
OBJDUMP	= $(GCCPREFIX)objdump
NM	= $(GCCPREFIX)nm
STRIP	= $(GCCPREFIX)strip

# Native commands
HOSTCC	= gcc
TAR	= tar
PERL	= perl

# Check for i386-jos-elf compiler
ifndef GCCPREFIX
GCCPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1 && i386-jos-elf-gcc -E -x c /dev/null >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake GCCPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# Compiler flags
# -Os is required for the boot loader to fit within 512 bytes;
# -ffreestanding means there is no standard library.
CFLAGS	:= $(CFLAGS) $(DEFS) \
	-std=gnu99 -m32 -ffunction-sections \
	-ffreestanding -I. -nostdinc -fno-omit-frame-pointer \
	-Wall -Wno-format -Wno-unused -Werror -ggdb -nostdinc
# Include -fno-stack-protector if the option exists.
CFLAGS	+= $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Linker flags
LDFLAGS	:= $(LDFLAGS) -Os --gc-sections
# Link for 32-bit targets if on x86_64.
LDFLAGS	+= $(shell $(LD) -m elf_i386 --help >/dev/null 2>&1 && echo -m elf_i386)


# Object directory and dependencies
$(OBJDIR)/stamp $(DEPSDIR)/stamp:
	$(call run,mkdir -p $(@D))
	$(call run,touch $@)

DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif


# Qemu emulator
INFERRED_QEMU := $(shell if which qemu-system-i386 2>/dev/null | grep ^/ >/dev/null 2>&1; \
	then echo qemu-system-i386; \
	elif grep 16 /etc/fedora-release >/dev/null 2>&1; \
	then echo qemu; else echo qemu-system-i386; fi)
QEMU ?= $(INFERRED_QEMU)
QEMUOPT	= -net none -parallel file:log.txt
QEMUCONSOLE ?= $(if $(DISPLAY),,1)
QEMUDISPLAY = $(if $(QEMUCONSOLE),console,graphic)

QEMU_PRELOAD_LIBRARY = $(OBJDIR)/libqemu-nograb.so.1

$(QEMU_PRELOAD_LIBRARY): build/qemu-nograb.c
	-$(call run,$(HOSTCC) -fPIC -shared -Wl$(comma)-soname$(comma)$(@F) -ldl -o $@ $<)

QEMU_PRELOAD = $(shell if test -r $(QEMU_PRELOAD_LIBRARY); then echo LD_PRELOAD=$(QEMU_PRELOAD_LIBRARY); fi)


# Run the emulator

run: run-qemu-$(basename $(IMAGE))
run-qemu: run-qemu-$(basename $(IMAGE))
run-graphic: run-graphic-$(basename $(IMAGE))
run-console: run-console-$(basename $(IMAGE))
run-gdb: run-gdb-$(basename $(IMAGE))
run-gdb-graphic: run-gdb-graphic-$(basename $(IMAGE))
run-gdb-console: run-gdb-console-$(basename $(IMAGE))
run-graphic-gdb: run-gdb-graphic-$(basename $(IMAGE))
run-console-gdb: run-gdb-console-$(basename $(IMAGE))
run-bochs: run-bochs-$(basename $(IMAGE))


check-qemu: $(OBJDIR)/libqemu-nograb.so.1
	@if test $$(whoami) = jharvard -a -z "$$(which $(QEMU) 2>/dev/null)"; then \
	    echo 1>&2; echo "***" 1>&2; \
	    echo "*** Cannot run $(QEMU). You may not have installed it yet." 1>&2; \
	    echo "*** I am going to try to install it for you." 1>&2; \
	    echo "***" 1>&2; echo 1>&2; \
	    echo sudo yum install -y qemu-system-x86; \
	    sudo yum install -y qemu-system-x86 || exit 1; \
	else :; fi

run-%: run-qemu-%
	@:

run-qemu-%: run-$(QEMUDISPLAY)-%
	@:

run-graphic-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -hda $<,QEMU $<)

run-console-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -curses -hda $<,QEMU $<)

run-gdb-%: run-gdb-$(QEMUDISPLAY)-%
	@:

run-gdb-graphic-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -S -gdb tcp::1234 -hda $< &,QEMU $<)
	$(call run,gdb -x .gdbinit,GDB)

run-gdb-console-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -curses -S -gdb tcp::1234 -hda $<,QEMU $<)


bochsrc-%:
	$(call run,/bin/mv .bochsrc .bochsrc~)
	$(call run,i=`echo $@ | sed s/bochsrc-//`; sed 's/path="[^"]*"/path="'$$i'.img"/' <.bochsrc~ >.bochsrc)
	$(call run,/bin/rm .bochsrc~)

run-bochs-%: % bochsrc-%
	$(call run,bochs -q,BOCHS $<)

# Kill all my qemus
kill:
	-killall -u $$(whoami) $(QEMU)
	@sleep 0.2; if ps -U $$(whoami) | grep $(QEMU) >/dev/null; then killall -9 -u $$(whoami) $(QEMU); fi


# Delete the build
clean:
	$(call run,rm -rf $(OBJDIR) *.img bochs.log core *.core,CLEAN)

realclean: clean
	$(call run,rm -rf $(DISTDIR)-$(USER).tar.gz $(DISTDIR)-$(USER))

distclean: realclean
	@:


# Boilerplate
always:
	@:

# These targets don't correspond to files
.PHONY: all always clean realclean distclean \
	run run-qemu run-graphic run-console run-gdb \
	run-gdb-graphic run-gdb-console run-graphic-gdb run-console-gdb \
	check-qemu kill \
	run-% run-qemu-% run-graphic-% run-console-% \
	run-gdb-% run-gdb-graphic-% run-gdb-console-% \
	run-bochs-% bochsrc-% run-bochs

# Eliminate default suffix rules
.SUFFIXES:

# Keep intermediate files
.SECONDARY:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# But no intermediate .o files should be deleted
.PRECIOUS: %.o $(OBJDIR)/%.o $(OBJDIR)/%.full $(OBJDIR)/bootsector
