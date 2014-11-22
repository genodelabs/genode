LIB_DIR     = $(REP_DIR)/src/lib/libnl
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += libc

LIBNL_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/libnl

INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(LIBNL_CONTRIB_DIR)/include

SRC_CC += lxcc_emul.cc socket.cc if.cc

# libnl
SRC_C += $(addprefix lib/, attr.c cache.c cache_mngt.c data.c error.c handlers.c \
                           hashtable.c msg.c nl.c object.c socket.c utils.c)

# libnl-genl
SRC_C += $(addprefix lib/genl/, ctrl.c family.c genl.c mngt.c)

CC_OPT   += -DSYSCONFDIR=\"/\"
CC_C_OPT += -include $(LIB_INC_DIR)/libnl_emul.h

# libnl-route
#SRC_C += $(wildcard lib/fib_lookup/*.c)   \
#         $(wildcard lib/route/act/*.c)    \
#         $(wildcard lib/route/cls/*.c)    \
#         $(wildcard lib/route/link/*.c)   \
#         $(wildcard lib/route/qdisc/*.c)  \
#         $(wildcard lib/route/*.c)

# libnl-netfilter
#SRC_C += $(wildcard lib/netfilter/*.c)

# libnl-diag
#SRC_C += $(wildcard lib/idiag/*.c)

#
# Generate links for emulated header files in build directory
#
EMUL_INC      := $(shell pwd)/include
EMUL_INC_LIST := $(addprefix linux/, netdevice.h ethtool.h atm.h socket.h in_route.h) \
                 $(addprefix asm/, byteorder.h)
EMUL_INCLUDES := $(addprefix $(EMUL_INC)/,$(EMUL_INC_LIST))
INC_DIR       += $(EMUL_INC)

#
# Eagerly create directory. Otherwise it will not be picked up from
# INC_DIR because $(wildcard ...) does not return non-existent dirs.
#
$(shell mkdir -p $(EMUL_INC))

#
# Make sure to create the header symlinks prior building
#
$(SRC_C:.c=.o) $(SRC_CC:.cc=.o): $(EMUL_INCLUDES)

$(EMUL_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(LIB_INC_DIR)/libnl_emul.h $@

vpath %.c  $(LIBNL_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)

# vi: set ft=make :
