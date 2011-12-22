TARGET         = dummy
LIBS           = oklx
REQUIRES       = okl4
SRC_CC         = main.cc
VERBOSE_LX_MK ?= 0
VMLINUX        = $(BUILD_BASE_DIR)/oklinux/vmlinux

$(TARGET): $(VMLINUX) $(BUILD_BASE_DIR)/bin/vmlinux

$(BUILD_BASE_DIR)/bin/vmlinux:
	$(VERBOSE)ln -s $(VMLINUX) $(BUILD_BASE_DIR)/bin/.

WOMBAT_CFLAGS = -I$(BUILD_BASE_DIR)/include -I$(REP_DIR)/include/oklx_lib \
                -I$(REP_DIR)/include/oklx_kernel -DMAX_ELF_SEGMENTS=1000 \
                -DCONFIG_MAX_THREAD_BITS=10 -D__i386__ $(CC_MARCH)

#
# The Linux kernel comes with its own declarations of typically builtin
# functions. Unfortunately, gcc-4.6.1 complains that the size type used in
# these declarations does not correspond to the compiler's __SIZE_TYPE__. This
# causes a flood of compile warnings. By marking said functions as non-builtin,
# those warnings are suppressed.
#
WOMBAT_CFLAGS += $(addprefix -fno-builtin-,strncpy strncat strncmp strlen memmove \
                                           memchr snprintf vsnprintf strncasecmp \
                                           strspn strcspn memcmp)

# disable noise caused by uncritical warnings
WOMBAT_CFLAGS += -Wno-unused-but-set-variable

$(VMLINUX): $(BUILD_BASE_DIR)/oklinux/.config
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR)  \
	               -C $(REP_DIR)/contrib vmlinux \
	               ARCH=l4 O=$(BUILD_BASE_DIR)/oklinux \
	               CROSS_COMPILE="$(CROSS_DEV_PREFIX)" \
	               WOMBAT_CFLAGS='$(WOMBAT_CFLAGS)' \
	               LDFLAGS='$(LD_MARCH)' \
	               LINK_ADDRESS=b000000 V=1 SYSTEM=i386 \
	               GENODE_LIBS_DIR=$(BUILD_BASE_DIR)/var/libcache \
	               GENODE_SRC_DIR=$(GENODE_DIR) \
	               KBUILD_VERBOSE=$(VERBOSE_LX_MK) \
	               V=$(VERBOSE_LX_MK)

$(BUILD_BASE_DIR)/oklinux/.config:
	$(VERBOSE)cp $(REP_DIR)/config/linux_config $@

clean: clean_oklinux

clean_oklinux:
	$(VERBOSE)rm -rf .t* ..t* .v* .missing-syscalls.d * .config

.PHONY: $(VMLINUX)

