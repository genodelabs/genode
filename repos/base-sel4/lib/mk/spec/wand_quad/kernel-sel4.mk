SEL4_DIR := $(call select_from_ports,sel4)/src/kernel/sel4
ELFLOADER_DIR := $(call select_from_ports,sel4_elfloader)/src/tool/elfloader

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
	$(VERBOSE)cp -rf $(ELFLOADER_DIR) elfloader && \
	          cd elfloader && \
	          sed -i "s/define UART_PPTR.*IMX6_UART2_PADDR/define UART_PPTR IMX6_UART1_PADDR/" src/arch-arm/plat-imx6/platform.h && \
	          $(MAKE) \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          NK_ASFLAGS=-DARMV7_A \
	          ARCH=arm PLAT=imx6 ARMV=armv7-a \
	          SEL4_COMMON=$(LIB_CACHE_DIR)/$(LIB)/elfloader \
	          SOURCE_DIR=$(LIB_CACHE_DIR)/$(LIB)/elfloader \
	          STAGE_DIR=$(LIB_CACHE_DIR)/$(LIB)/elfloader \
	          srctree=$(LIB_CACHE_DIR)/$(LIB)/elfloader

build_kernel: elfloader/elfloader.o
	$(VERBOSE)$(MAKE) \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          BOARD=wand_quad ARCH=arm PLAT=imx6 CPU=cortex-a9 ARMV=armv7-a DEBUG=1 \
	          SOURCE_ROOT=$(SEL4_DIR) -f$(SEL4_DIR)/Makefile

