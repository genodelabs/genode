GDB_CONTRIB_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

INC_DIR += $(REP_DIR)/src/lib/gdbserver_libc_support \
           $(REP_DIR)/src/lib/gdbserver_platform \
           $(REP_DIR)/src/app/gdb_monitor \
           $(GDB_CONTRIB_DIR)/include \
           $(GDB_CONTRIB_DIR)/gdb \
           $(GDB_CONTRIB_DIR)/gdb/common \
           $(GDB_CONTRIB_DIR)/gdb/gdbserver \
           $(GDB_CONTRIB_DIR)/gdb/gnulib/import \
           $(GDB_CONTRIB_DIR)/gdb/regformats \
           $(REP_DIR)/src/app/gdb_monitor/gdbserver

SRC_CC += gdbserver_platform_helper.cc

CC_OPT_gdbserver_platform_helper += -fpermissive

CC_OPT += -DGDBSERVER
CC_OPT += -DHAVE_DECL_PTRACE -DHAVE_SYS_PTRACE_H

LIBS += stdcxx libc

vpath %.cc $(REP_DIR)/src/lib/gdbserver_platform
vpath %.cc  $(GDB_CONTRIB_DIR)/gdb
