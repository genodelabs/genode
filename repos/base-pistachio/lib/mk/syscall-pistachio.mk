PISTACHIO_CONTRIB_DIR := $(call select_from_ports,pistachio)/src/kernel/pistachio
PISTACHIO_USER_SRC    := $(PISTACHIO_CONTRIB_DIR)/user/lib/l4

LD_PREFIX := "-Wl,"

CC_WARN += -Wno-array-bounds -Wno-unused-but-set-variable \
           -Wno-parentheses -Wno-format

user_build.tag:
	LIBGCCFLAGS="$(CC_MARCH)" \
	LDFLAGS="$(addprefix $(LD_PREFIX),$(LD_MARCH)) -nostdlib" \
	CFLAGS="$(CC_MARCH) $(CC_WARN)" \
	$(PISTACHIO_CONTRIB_DIR)/user/configure --build=ia32 --host i686 \
	                                        CC=$(CROSS_DEV_PREFIX)gcc
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) -s $(VERBOSE_DIR)
	@touch $@

SRC_CC := debug.cc ia32.cc
SRC_S  := ia32-syscall-stubs.S
CC_OPT += -Iinclude

$(SRC_CC:.cc=.o) $(SRC_S:.S:.o): user_build.tag

vpath % $(PISTACHIO_USER_SRC)
