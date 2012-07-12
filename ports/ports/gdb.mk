GDB_VERSION  = 7.3.1
GDB          = gdb-$(GDB_VERSION)
GDB_URL      = ftp://ftp.fu-berlin.de/gnu/gdb
GDB_TBZ2     = gdb-$(GDB_VERSION).tar.bz2

# these files are only needed to generate other files in the preparation process
GDB_CONTENT := gdb/regformats/regdat.sh \
               gdb/regformats/reg-arm.dat \
               gdb/regformats/i386/i386.dat \
               gdb/regformats/i386/i386-avx.dat \
               gdb/regformats/regdef.h \
               gdb/common/ax.def \
               gdb/common/i386-xstate.h \
               gdb/common/gdb_signals.h \
               gdb/common/signals.c \
               gdb/gdbserver/i386-low.c \
               gdb/gdbserver/i386-low.h \
               gdb/gdbserver/i387-fp.c \
               gdb/gdbserver/i387-fp.h \
               gdb/gdbserver/linux-arm-low.c \
               gdb/gdbserver/linux-low.c \
               gdb/gdbserver/linux-low.h \
               gdb/gdbserver/linux-x86-low.c \
               gdb/gdbserver/utils.c \
               gdb/gdbserver/regcache.c \
               gdb/gdbserver/target.h \
               gdb/gdbserver/regcache.h \
               gdb/gdbserver/server.c \
               gdb/gdbserver/remote-utils.c \
               gdb/gdbserver/mem-break.h \
               gdb/gdbserver/target.c \
               gdb/gdbserver/event-loop.c \
               gdb/gdbserver/inferiors.c \
               gdb/gdbserver/tracepoint.c \
               gdb/gdbserver/server.h \
               gdb/gdbserver/terminal.h \
               gdb/gdbserver/mem-break.c \
               include/gdb/signals.def \
               include/gdb/signals.h

#
# Interface to top-level prepare Makefile
#
PORTS += $(GDB)

prepare:: $(CONTRIB_DIR)/$(GDB)/configure generated_files

#
# Port-specific local rules
#

$(DOWNLOAD_DIR)/$(GDB_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GDB_URL)/$(GDB_TBZ2) && touch $@

$(CONTRIB_DIR)/$(GDB): $(DOWNLOAD_DIR)/$(GDB_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR)

include ../tool/tool_chain_gdb_patches.inc

$(CONTRIB_DIR)/$(GDB)/configure:: $(CONTRIB_DIR)/$(GDB)
	@#
	@# Genode-specific changes
	@#
	$(VERBOSE)patch -N -p1 -d $(CONTRIB_DIR)/$(GDB) < src/app/gdb_monitor/gdbserver_genode.patch
	$(VERBOSE)patch -N -p1 -d $(CONTRIB_DIR)/$(GDB) < src/noux-pkg/gdb/build.patch

GENERATED_DIR := src/lib/gdbserver_platform/generated

$(GENERATED_DIR):
	$(VERBOSE) mkdir $@

REGFORMATS_DIR := $(abspath $(CONTRIB_DIR)/$(GDB)/gdb/regformats)

$(GENERATED_DIR)/reg-arm.c: $(GENERATED_DIR)
	$(VERBOSE) cd $(GENERATED_DIR) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/reg-arm.dat $(notdir $@)

$(GENERATED_DIR)/i386.c: $(GENERATED_DIR)
	$(VERBOSE) cd $(GENERATED_DIR) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/i386/i386.dat $(notdir $@)

$(GENERATED_DIR)/i386-avx.c: $(GENERATED_DIR)
	$(VERBOSE) cd $(GENERATED_DIR) && $(SHELL) $(REGFORMATS_DIR)/regdat.sh $(REGFORMATS_DIR)/i386/i386-avx.dat $(notdir $@)

generated_files: $(CONTRIB_DIR)/$(GDB) \
                 $(GENERATED_DIR)/reg-arm.c \
                 $(GENERATED_DIR)/i386.c \
                 $(GENERATED_DIR)/i386-avx.c

.PHONY: generated_files
