GDB_CONTRIB_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

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

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform
vpath %.c  $(REP_DIR)/src/lib/gdbserver_platform/generated
vpath %.c  $(GDB_CONTRIB_DIR)/gdb/gdbserver

#
# Detect missing preparation of GDB package
#
ifeq ($(wildcard $(GDB_CONTRIB_DIR)),)
REQUIRES = prepare_ports_gdb
endif
