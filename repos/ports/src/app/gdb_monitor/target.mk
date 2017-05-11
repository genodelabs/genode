TARGET  =  gdb_monitor

GDB_CONTRIB_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

INC_DIR += $(GDB_CONTRIB_DIR)/include \
           $(GDB_CONTRIB_DIR)/gdb/common \
           $(GDB_CONTRIB_DIR)/gdb/gdbserver \
           $(GDB_CONTRIB_DIR)/gdb/regformats \
           $(REP_DIR)/src/lib/gdbserver_libc_support \
           $(PRG_DIR)/gdbserver \
           $(PRG_DIR)

LIBS    =  libc libc_terminal libc_pipe \
           gdbserver_platform gdbserver_libc_support

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

CC_OPT += -DGDBSERVER -DPKGVERSION="\"7.3.1\"" -DREPORT_BUGS_TO="\"\""

CC_OPT_linux-low += -Wno-unused-function

SRC_CC  =  genode-low.cc \
           cpu_session_component.cc \
           cpu_thread_component.cc \
           region_map_component.cc \
           signal_handler_thread.cc \
           main.cc

vpath % $(GDB_CONTRIB_DIR)/gdb/common
vpath % $(GDB_CONTRIB_DIR)/gdb/gdbserver
vpath % $(PRG_DIR)/gdbserver
