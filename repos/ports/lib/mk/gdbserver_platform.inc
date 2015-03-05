GDB_CONTRIB_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb
GDBSERVER_PLATFORM_GENERATED_DIR = $(call select_from_ports,gdb)/src/lib/gdbserver_platform/generated

INC_DIR += $(REP_DIR)/src/lib/gdbserver_libc_support \
           $(REP_DIR)/src/lib/gdbserver_platform \
           $(REP_DIR)/src/app/gdb_monitor \
           $(GDB_CONTRIB_DIR)/include \
           $(GDB_CONTRIB_DIR)/gdb/common \
           $(GDB_CONTRIB_DIR)/gdb/gdbserver \
           $(GDB_CONTRIB_DIR)/gdb/regformats \
           $(REP_DIR)/src/app/gdb_monitor/gdbserver

SRC_CC += gdbserver_platform_helper.cc

LIBS += libc

# generated files

$(GDBSERVER_PLATFORM_GENERATED_DIR):
	$(VERBOSE) mkdir -p $@

REGFORMATS_DIR := $(abspath ${GDB_CONTRIB_DIR}/gdb/regformats)

$(GDBSERVER_PLATFORM_GENERATED_DIR)/reg-arm.c: $(GDBSERVER_PLATFORM_GENERATED_DIR)
	$(VERBOSE) cd $(dir $@) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/reg-arm.dat $(notdir $@)

$(GDBSERVER_PLATFORM_GENERATED_DIR)/i386.c: $(GDBSERVER_PLATFORM_GENERATED_DIR)
	$(VERBOSE) cd $(dir $@) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/i386/i386.dat $(notdir $@)

$(GDBSERVER_PLATFORM_GENERATED_DIR)/i386-avx.c: $(GDBSERVER_PLATFORM_GENERATED_DIR)
	$(VERBOSE) cd $(dir $@) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/i386/i386-avx.dat $(notdir $@)

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform
vpath %.c  $(GDBSERVER_PLATFORM_GENERATED_DIR)
vpath %.c  $(GDB_CONTRIB_DIR)/gdb/gdbserver

#
# Detect missing preparation of GDB package
#
ifeq ($(wildcard $(GDB_CONTRIB_DIR)),)
REQUIRES = prepare_ports_gdb
endif
