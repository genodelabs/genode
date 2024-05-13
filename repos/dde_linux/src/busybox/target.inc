TARGET := busybox

CUSTOM_TARGET_DEPS := busybox.links

BB_DIR := $(call select_from_ports,busybox)/src/busybox
PWD    := $(shell pwd)

# do not confuse third-party sub-makes
unexport .SHELLFLAGS

# filter for make output of kernel build system
BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [Busybox]  /"

busybox.links:
	$(MSG_CONFIG)Buildroot
	$(VERBOSE)$(MAKE) -C $(BB_DIR) O=$(PWD) $(BB_MK_ARGS) defconfig $(BUILD_OUTPUT_FILTER)
	$(VERBOSE)sed -i -e 's/^# CONFIG_STATIC.*/CONFIG_STATIC=y/' .config
	$(MSG_BUILD)Buildroot
	$(VERBOSE)$(MAKE) $(BB_MK_ARGS) $(BUILD_OUTPUT_FILTER)
	$(VERBOSE)$(MAKE) $(BB_MK_ARGS) CONFIG_PREFIX=rootfs install $(BUILD_OUTPUT_FILTER)
