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

ifeq ($(CCACHE),yes)
SEL4_CCACHE=ccache
else
SEL4_CCACHE=
endif

configured_kernel:
	$(VERBOSE) cmake -DCROSS_COMPILER_PREFIX=$(CROSS_DEV_PREFIX) \
	                 -DCMAKE_TOOLCHAIN_FILE=$(SEL4_DIR)/gcc.cmake \
	                 -DKernelARMPlatform=$(ARM_PLATFORM) \
	                 -DKernelPlatform=$(ARM_PLATFORM) \
	                 -G Ninja \
	                 -C $(SEL4_DIR)/configs/AARCH64_verified.cmake \
	                 $(SEL4_DIR) \
	&& echo -e "\n#define CONFIG_PRINTING 1"                         >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_DEBUG_BUILD 1"                        >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_ENABLE_BENCHMARKS 1"                  >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_BENCHMARK_TRACK_UTILISATION 1"        >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_SET_TLS_BASE_SELF 1"                  >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_ALLOW_SMC_CALLS 1"                    >>gen_config/kernel/gen_config.h \
	\
	&& sed -e "s/CONFIG_NUM_DOMAINS  16/CONFIG_NUM_DOMAINS  1/"                    \
	       -e "s/CONFIG_ROOT_CNODE_SIZE_BITS  12/CONFIG_ROOT_CNODE_SIZE_BITS  15/" \
	       -e "s/CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS  50/CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS  160/" \
	          gen_config/kernel/gen_config.h   >gen_config/kernel/gen_config.tmp   \
	&& mv     gen_config/kernel/gen_config.tmp  gen_config/kernel/gen_config.h     \
	&& touch configured_kernel

elfloader/elfloader.o: configured_kernel
	$(VERBOSE)cp -r $(TOOLS_DIR)/elfloader-tool $(LIB_CACHE_DIR)/$(LIB)/elfloader
	$(VERBOSE)mkdir -p $(LIB_CACHE_DIR)/$(LIB)/elfloader/tools/kbuild
	$(VERBOSE)mkdir -p $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/generated
	$(VERBOSE)mkdir -p $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/elfloader
	$(VERBOSE)ln -sf $(TOOLS_DIR)/common-tool/common.mk $(LIB_CACHE_DIR)/$(LIB)/elfloader/
	$(VERBOSE)ln -sf $(TOOLS_DIR)/common-tool/files_to_obj.sh $(LIB_CACHE_DIR)/$(LIB)/elfloader/
	$(VERBOSE)ln -sf $(TOOLS_DIR)/kbuild-tool/Kbuild.include $(LIB_CACHE_DIR)/$(LIB)/elfloader/tools/kbuild/
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/$(LIB)/gen_config/kernel/gen_config.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/generated/autoconf.h
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/$(BOARD)/gen_config.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/elfloader/.
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/$(BOARD)/devices_gen.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/.
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/$(BOARD)/image_start_addr.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/.
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/$(LIB)/elfloader/include/arch-arm/$(BOARD)/image_start_addr.h $(LIB_CACHE_DIR)/$(LIB)/elfloader/src/arch-arm/.
	$(VERBOSE)$(MAKE) -C $(LIB_CACHE_DIR)/$(LIB)/elfloader \
	          TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	          ARCH=arm PLAT=$(PLAT) ARMV=armv8-a __ARM_64__="y" \
	          CPU=$(CPU) ASFLAGS="-march=armv8-a" \
	          CFLAGS="-march=armv8-a+nofp+nosimd -D__KERNEL_64__ -fno-builtin -mgeneral-regs-only -mstrict-align" \
	          SEL4_COMMON=. CCACHE=$(SEL4_CCACHE) SOURCE_DIR=. STAGE_DIR=. \
	          srctree=.

build_kernel: elfloader/elfloader.o
	$(VERBOSE) ninja kernel.elf && \
	$(CUSTOM_STRIP) -o kernel.elf.strip kernel.elf
