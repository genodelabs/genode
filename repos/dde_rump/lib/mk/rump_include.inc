ifeq ($(called_from_lib_mk),yes)

include $(REP_DIR)/lib/mk/rump_common.inc


$(RUMP_BASE):
	mkdir -p $@

$(RUMP_BASE)/include/machine: $(RUMP_BASE)
	$(VERBOSE_MK)mkdir -p $(RUMP_BASE)/include
	$(VERBOSE_MK)for arch in $(INC_ARCH); do \
		ln -sf $(RUMP_PORT_DIR)/src/sys/arch/$$arch/include $(RUMP_BASE)/include/$$arch ; done
	$(VERBOSE_MK)touch -a $(RUMP_BASE)/include/pthread_types.h
	$(VERBOSE_MK)ln -sf $(INC_MACHINE) $(RUMP_BASE)/include/machine

all: $(RUMP_BASE)/include/machine

endif

# vi:set ft=make :
