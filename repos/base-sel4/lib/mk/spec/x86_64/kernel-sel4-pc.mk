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

configured_kernel:
	$(VERBOSE) cmake -DCROSS_COMPILER_PREFIX=$(CROSS_DEV_PREFIX) \
	                 -DCMAKE_TOOLCHAIN_FILE=$(SEL4_DIR)/gcc.cmake \
	                 -G Ninja \
	                 -C $(SEL4_DIR)/configs/X64_verified.cmake \
	                 $(SEL4_DIR) \
	\
	&& echo -e "\n#define CONFIG_PRINTING 1"                  >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_DEBUG_BUILD 1"                 >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_VTX 1"                         >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_ENABLE_BENCHMARKS 1"           >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_BENCHMARK_TRACK_UTILISATION 1" >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_ARCH_X86_GENERIC 1"            >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_FXSAVE           1"            >>gen_config/kernel/gen_config.h \
	&& echo -e "#define CONFIG_SET_TLS_BASE_SELF 1"           >>gen_config/kernel/gen_config.h \
	\
	&& sed -e "s/CONFIG_MAX_NUM_NODES 1/CONFIG_MAX_NUM_NODES 16/"                                   \
	       -e "s/CONFIG_MAX_VPIDS 0/CONFIG_MAX_VPIDS 64/"                                           \
	       -e "s/CONFIG_NUM_DOMAINS 16/CONFIG_NUM_DOMAINS 1/"                                       \
	       -e "s/CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS 50/CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS 160/" \
	       -e "s/CONFIG_FSGSBASE_INST 1/CONFIG_FSGSBASE_MSR 1/"                                     \
	       -e "s/CONFIG_KERNEL_FSGS_BASE inst/CONFIG_KERNEL_FSGS_BASE msr/"                         \
	       -e "/CONFIG_HUGE_PAGE/d"                                                                 \
	       -e "/CONFIG_SUPPORT_PCID/d"                                                              \
	       -e "/CONFIG_XSAVE 1/d"                                                                   \
	       -e "/CONFIG_XSAVE_XSAVEOPT 1/d"                                                          \
	       gen_config/kernel/gen_config.h   >gen_config/kernel/gen_config.tmp \
	&& mv  gen_config/kernel/gen_config.tmp  gen_config/kernel/gen_config.h \
	&& touch configured_kernel

build_kernel: configured_kernel
	$(VERBOSE) ninja kernel.elf && \
	$(CUSTOM_STRIP) -o kernel.elf.strip kernel.elf
