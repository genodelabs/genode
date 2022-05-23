#
# This library is for avoiding clashes between Linux and Genode includes.
# If we were to compile all the *.c files of this library directly with the
# WireGuard library, there would be only one INC_DIR applied to *.cc files as
# well as to *.c files. Some Genode headers, however, (e.g. 'net/udp.h' or
# 'net/dhcp.h') exist with the same include identifier in both Linux and
# Genode, and we would have the problem that we want the Linux variant in *.c
# files and the Genode variant in *.cc files. Therefore, we create two
# dedicated INC_DIR settings through separate libraries.
#

GEN_PRG_DIR := $(PRG_DIR)/../..

SRC_C += $(notdir $(wildcard $(PRG_DIR)/generated_dummies.c))
SRC_C += dummies.c
SRC_C += dummies_arch.c
SRC_C += lx_emul.c
SRC_C += wireguard.c
SRC_C += genode_c_api_arch.c

vpath wireguard.c $(GEN_PRG_DIR)/genode_c_api
vpath %.c         $(PRG_DIR)
vpath %.c         $(GEN_PRG_DIR)

INC_DIR += $(PRG_DIR)
INC_DIR += $(GEN_PRG_DIR)

LIBS += lx_emul virt_linux_generated

#
# We shadow some files of lx_emul and lx_kit in order to locally apply
# modifications.
#

vpath lx_emul/alloc.cc $(GEN_PRG_DIR)
vpath lx_kit/memory.cc $(GEN_PRG_DIR)

#
# Some local files include linux headers that expect the KBUILD_* symbols to
# be defined. However, the lx_emul mechanism for adding the definitions doesn't
# apply to these local files, which is why we have to add them manually.
#

CC_OPT_wireguard += -DKBUILD_MODFILE='"wireguard"' \
                    -DKBUILD_BASENAME='"wireguard"' \
                    -DKBUILD_MODNAME='"wireguard"'

CC_OPT_lx_emul += -DKBUILD_MODFILE='"lx_emul"' \
                  -DKBUILD_BASENAME='"lx_emul"' \
                  -DKBUILD_MODNAME='"lx_emul"'

#
# The lx_emul Makefiles have a generic mechanism for defining the
# KBUILD_MODNAME for each Linux unit to be the filename of that unit
# (without suffix). However, some WireGuard units expect
# KBUILD_MODNAME to be the same for all WireGuard units.
# Therefore, we do a pattern substition on the CC_OPT_* variables of
# those units, changing their KBUILD_MODNAME def to "wireguard".
#

OBJECTS_TO_FIX_MODNAME_DEFS_FOR := \
	drivers/net/wireguard/device.o \
	drivers/net/wireguard/netlink.o

define FIX_MODNAME_DEF =
CC_OPT_$(1) = $(patsubst -DKBUILD_MODNAME='"%"',-DKBUILD_MODNAME='"wireguard"',$(CC_OPT_$(1)))
endef

.PHONY: fix_modname_defs
fix_modname_defs:
	$(foreach file,$(OBJECTS_TO_FIX_MODNAME_DEFS_FOR), \
		$(eval $(call FIX_MODNAME_DEF,$(file:%.o=%))))

$(OBJECTS_TO_FIX_MODNAME_DEFS_FOR): fix_modname_defs
