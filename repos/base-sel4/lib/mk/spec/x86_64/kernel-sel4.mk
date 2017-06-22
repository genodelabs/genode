SEL4_DIR := $(call select_from_ports,sel4)/src/kernel/sel4

#
# Execute the kernel build only at the second build stage when we know
# about the complete build settings (e.g., the 'CROSS_DEV_PREFIX') and the
# current working directory is the library location.
#
ifeq ($(called_from_lib_mk),yes)
all: build_kernel
else
all:
endif

build_kernel:
	$(VERBOSE)$(MAKE) \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          BOARD=x86_64 ARCH=x86 SEL4_ARCH=x86_64 PLAT=pc99 DEBUG=1 \
	          SOURCE_ROOT=$(SEL4_DIR) -f$(SEL4_DIR)/Makefile

