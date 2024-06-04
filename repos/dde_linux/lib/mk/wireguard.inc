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

SRC_C += $(notdir $(wildcard $(PRG_DIR)/generated_dummies.c))
SRC_C += dummies.c
SRC_C += lx_emul.c
SRC_C += wireguard.c
SRC_C += genode_c_api_arch.c

vpath wireguard.c $(PRG_TOP_DIR)/genode_c_api
vpath %.c         $(PRG_TOP_DIR)
vpath %.c         $(PRG_DIR)

INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_TOP_DIR)

LIBS += virt_lx_emul virt_linux_generated

#
# Some local files include linux headers that expect the KBUILD_* symbols to
# be defined. However, the lx_emul mechanism for adding the definitions doesn't
# apply to these files, which is why we have to do it manually.
#

WG_KBUILD_DEFS := -DKBUILD_MODFILE='"wireguard"' \
                  -DKBUILD_BASENAME='"wireguard"' \
                  -DKBUILD_MODNAME='"wireguard"'

CC_OPT_wireguard += $(WG_KBUILD_DEFS)
CC_OPT_lx_emul   += $(WG_KBUILD_DEFS)

#
# The lx_emul Makefiles have a generic mechanism for defining the
# KBUILD_MODFILE, KBUILD_BASENAME, and KBUILD_MODNAME for each Linux
# compilation unit to be unique. However, some WireGuard units expect
# KBUILD_MODNAME to be the same for all WireGuard units. Therefore, we
# let a target-specific variable override the CC_OPT_* variable for those
# units.
#

OBJECTS_TO_FIX_CC_OPT_FOR := \
	drivers/net/wireguard/device.o \
	drivers/net/wireguard/netlink.o

define FIX_OBJECT_CC_OPT =
$(1): override CC_OPT_$(1:%.o=%) = $(WG_KBUILD_DEFS)
endef

$(foreach OBJECT,$(OBJECTS_TO_FIX_CC_OPT_FOR), \
	$(eval $(call FIX_OBJECT_CC_OPT,$(OBJECT))))
