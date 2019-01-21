SEL4_DIR  := $(call select_from_ports,sel4)/src/kernel/sel4
TOOLS_DIR := $(call select_from_ports,sel4_tools)/src/tool/sel4_tools

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

elfloader/elfloader.o:
	$(VERBOSE)cp -r $(TOOLS_DIR)/elfloader-tool $(LIB_CACHE_DIR)/$(LIB)/elfloader
	$(VERBOSE)mkdir -p $(LIB_CACHE_DIR)/$(LIB)/elfloader/tools/kbuild
	$(VERBOSE)mkdir -p $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/generated
	$(VERBOSE)ln -s $(TOOLS_DIR)/common-tool/common.mk $(LIB_CACHE_DIR)/$(LIB)/elfloader/
	$(VERBOSE)ln -s $(TOOLS_DIR)/common-tool/files_to_obj.sh $(LIB_CACHE_DIR)/$(LIB)/elfloader/
	$(VERBOSE)ln -s $(TOOLS_DIR)/kbuild-tool/Kbuild.include $(LIB_CACHE_DIR)/$(LIB)/elfloader/tools/kbuild/
	$(VERBOSE)ln -s $(SEL4_DIR)/configs/$(PLAT)/$(BOARD)/autoconf.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/generated/
	$(VERBOSE)$(MAKE) -C $(LIB_CACHE_DIR)/$(LIB)/elfloader \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          ARCH=arm PLAT=$(PLAT) ARMV=armv7-a __ARM_32__="y" \
	          CPU=$(CPU) ASFLAGS="-march=armv7-a" \
	          CFLAGS="-march=armv7-a -D__KERNEL_32__ -fno-builtin" \
	          SEL4_COMMON=.  SOURCE_DIR=.  STAGE_DIR=.  srctree=.

build_kernel: elfloader/elfloader.o
	$(VERBOSE)$(MAKE) \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          BOARD=$(BOARD) ARCH=arm PLAT=$(PLAT) CPU=$(CPU) ARMV=armv7-a DEBUG=1 \
	          SOURCE_ROOT=$(SEL4_DIR) -f$(SEL4_DIR)/Makefile
