TARGET  =  gdb_monitor

GDB_CONTRIB_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

INC_DIR += $(GDB_CONTRIB_DIR)/include \
           $(GDB_CONTRIB_DIR)/gdb \
           $(GDB_CONTRIB_DIR)/gdb/common \
           $(GDB_CONTRIB_DIR)/gdb/gdbserver \
           $(GDB_CONTRIB_DIR)/gdb/gnulib/import \
           $(GDB_CONTRIB_DIR)/gdb/regformats \
           $(REP_DIR)/src/lib/gdbserver_libc_support \
           $(PRG_DIR)/gdbserver \
           $(PRG_DIR)

LIBS    =  stdcxx libc libc_pipe \
           gdbserver_platform gdbserver_libc_support

# libiberty
SRC_C   = argv.c \
          concat.c \
          crc32.c

# gnulib
SRC_C  += rawmemchr.c \
          strchrnul.c

# gdb
SRC_CC += common/agent.cc \
          common/buffer.cc \
          common/cleanups.cc \
          common/common-debug.cc \
          common/common-exceptions.cc \
          common/common-utils.cc \
          common/environ.cc \
          common/errors.cc \
          common/filestuff.cc \
          common/format.cc \
          common/gdb_tilde_expand.cc \
          common/gdb_vecs.cc \
          common/job-control.cc \
          common/pathstuff.cc \
          common/posix-strerror.cc \
          common/print-utils.cc \
          common/ptid.cc \
          common/rsp-low.cc \
          common/signals.cc \
          common/tdesc.cc \
          common/vec.cc \
          common/xml-utils.cc \
          gdbserver/ax.cc \
          gdbserver/debug.cc \
          gdbserver/dll.cc \
          gdbserver/event-loop.cc \
          gdbserver/fork-child.cc \
          gdbserver/hostio.cc \
          gdbserver/i387-fp.cc \
          gdbserver/inferiors.cc \
          gdbserver/linux-low.cc \
          gdbserver/mem-break.cc \
          gdbserver/notif.cc \
          gdbserver/regcache.cc \
          gdbserver/remote-utils.cc \
          gdbserver/server.cc \
          gdbserver/symbol.cc \
          gdbserver/target.cc \
          gdbserver/tdesc.cc \
          gdbserver/tracepoint.cc \
          gdbserver/utils.cc \
          gdbserver/x86-low.cc \
          nat/fork-inferior.cc \
          nat/linux-ptrace.cc \
          nat/x86-dregs.cc \
          target/waitstatus.cc

# genode
SRC_CC += genode-low.cc \
          cpu_session_component.cc \
          cpu_thread_component.cc \
          region_map_component.cc \
          signal_handler_thread.cc \
          main.cc

CC_OPT += -DGDBSERVER -DPKGVERSION="\"8.1.1\"" -DREPORT_BUGS_TO="\"\""
CC_OPT += -DHAVE_SYS_WAIT_H -DHAVE_SYS_PTRACE_H -DHAVE_DECL_PTRACE -DHAVE_TERMIOS
CC_OPT += -fpermissive -Wno-unused-function

vpath %.c  $(GDB_CONTRIB_DIR)/gdb/gnulib/import
vpath %.c  $(GDB_CONTRIB_DIR)/libiberty
vpath %.cc $(GDB_CONTRIB_DIR)/gdb
vpath %.cc $(PRG_DIR)/gdbserver

#
# Files from init
#
# Because the 'server.h' file exists both in gdb and in init and both gdb's
# 'server.c' and init's 'server.cc' are compiled to a 'server.o' file, the
# parent directory of the init source is used as reference.
#

SANDBOX_PARENT_DIR = $(abspath $(dir $(call select_from_repositories,src/lib/sandbox/server.cc))/..)

INC_DIR += $(SANDBOX_PARENT_DIR)

SRC_CC += sandbox/server.cc

vpath sandbox/%.cc $(SANDBOX_PARENT_DIR)

CC_CXX_WARN_STRICT =
