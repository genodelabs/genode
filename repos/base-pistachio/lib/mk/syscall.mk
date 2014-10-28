PISTACHIO_USER_SRC := $(call select_from_ports,pistachio)/src/kernel/pistachio/user/lib/l4

SRC_CC = debug.cc ia32.cc
SRC_S  = ia32-syscall-stubs.S

vpath %.cc $(PISTACHIO_USER_SRC)
vpath %.S  $(PISTACHIO_USER_SRC)
