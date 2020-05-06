REQUIRES += 64bit

SHARED_LIB = yes

CC_OPT += -D__SOLO5_BINDINGS__ -Drestrict=__restrict__

# GCC 8 complains about a mismatch of the 'log()' function declaration.
# Since the solo5 'log()' function is unrelated to the builtin function,
# we disable the error message.
CC_OPT += -Wno-builtin-declaration-mismatch

SRC_CC = bindings.cc

SOLO5_PORT_DIR := $(call select_from_ports,solo5)

INC_DIR += $(SOLO5_PORT_DIR)/src/lib/solo5/bindings
INC_DIR += $(SOLO5_PORT_DIR)/include/solo5
INC_DIR += $(REP_DIR)/include/solo5

vpath bindings.cc $(SOLO5_PORT_DIR)/src/lib/solo5/bindings/genode

# ignore warning about use of deprecated Xml_node API
CC_CXX_WARN_STRICT :=
