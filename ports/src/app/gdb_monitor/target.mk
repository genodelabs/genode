TARGET  =  gdb_monitor

GDB_CONTRIB_DIR = $(REP_DIR)/contrib/gdb-7.3.1

INC_DIR += $(GDB_CONTRIB_DIR)/include \
           $(GDB_CONTRIB_DIR)/gdb/common \
           $(GDB_CONTRIB_DIR)/gdb/gdbserver \
           $(GDB_CONTRIB_DIR)/gdb/regformats \
           $(REP_DIR)/src/lib/gdbserver_libc_support \
           $(PRG_DIR)/gdbserver \
           $(PRG_DIR)

LIBS    =  env signal libc libc_log libc_terminal libc_lock_pipe lock process server gdbserver_platform gdbserver_libc_support

SRC_C   =  event-loop.c \
           i386-low.c \
           i387-fp.c \
           inferiors.c \
           mem-break.c \
           remote-utils.c \
           regcache.c \
           server.c \
           signals.c \
           target.c \
           tracepoint.c \
           utils.c

SRC_C  +=  linux-low.c

CC_OPT += -DGDBSERVER

CC_OPT_linux-low += -Wno-unused-function

SRC_CC  =  genode-low.cc \
           gdb_stub_thread.cc \
           cpu_session_component.cc \
           ram_session_component.cc \
           rm_session_component.cc \
           signal_handler_thread.cc \
           main.cc

vpath % $(GDB_CONTRIB_DIR)/gdb/common
vpath % $(GDB_CONTRIB_DIR)/gdb/gdbserver
vpath % $(PRG_DIR)/gdbserver
