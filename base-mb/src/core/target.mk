TARGET = core
LIBS   = kernel_core cxx ipc heap printf_microblaze child pager lock \
         raw_signal raw_server

STARTUP_LIB = kernel_core
LD_SCRIPT   = $(REP_DIR)/src/platform/genode.ld

SRC_S += boot_modules.s

include $(PRG_DIR)/target.inc
