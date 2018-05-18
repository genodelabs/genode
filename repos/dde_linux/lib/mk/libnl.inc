LIB_DIR     = $(REP_DIR)/src/lib/libnl
LIB_INC_DIR = $(LIB_DIR)/include

LIBS += libc libnl_include

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

CC_OPT += -D_LINUX_SOCKET_H

vpath %.c  $(LIBNL_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)

# vi: set ft=make :
