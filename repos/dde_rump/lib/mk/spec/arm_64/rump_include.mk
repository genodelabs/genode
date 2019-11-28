ifeq ($(called_from_lib_mk),yes)

include $(REP_DIR)/lib/mk/rump_common.inc

BACKPORT := $(call select_from_ports,dde_rump)/src/lib/dde_rump_aarch64_backport

$(RUMP_BASE):
	mkdir -p $@

$(RUMP_BASE)/include/machine: $(RUMP_BASE)
	$(VERBOSE_MK)mkdir -p $(RUMP_BASE)/include
	$(VERBOSE_MK)ln -sf $(BACKPORT)/arm/include $(RUMP_BASE)/include/arm
	$(VERBOSE_MK)ln -sf $(BACKPORT)/aarch64/include $(RUMP_BASE)/include/aarch64
	$(VERBOSE_MK)ln -sf $(BACKPORT)/evbarm64/include $(RUMP_BASE)/include/machine
	$(VERBOSE_MK)touch -a $(RUMP_BASE)/include/pthread_types.h

all: $(RUMP_BASE)/include/machine

endif

# vi:set ft=make :

CC_CXX_WARN_STRICT =
