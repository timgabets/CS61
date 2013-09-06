DEPSDIR := .deps
REBUILDSTAMP := $(DEPSDIR)/rebuildstamp

V = 0
ifeq ($(V),1)
run = $(1) $(3)
else
run = @$(if $(2),/bin/echo " " $(2) $(3) &&,) $(1) $(3)
endif

$(DEPSDIR)/stamp $(REBUILDSTAMP):
	$(call run,mkdir -p $(@D))
	$(call run,touch $@)

DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

clean-hook:
	@:


