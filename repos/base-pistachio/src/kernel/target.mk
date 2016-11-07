TARGET           = kernel
REQUIRES        += pistachio
KERNEL_BUILD_DIR = $(BUILD_BASE_DIR)/kernel/pistachio
KERNEL           = $(KERNEL_BUILD_DIR)/x86-kernel
KERNEL_SRC      := $(call select_from_ports,pistachio)/src/kernel/pistachio/kernel

LIBGCC_DIR = $(dir $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name))
GCCINC_DIR = $(dir $(shell $(CC) -print-libgcc-file-name))include

$(TARGET): $(KERNEL)

.PHONY: $(KERNEL)

$(KERNEL_BUILD_DIR)/Makefile:
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) $(VERBOSE_DIR) -C $(KERNEL_SRC) BUILDDIR=$(dir $@)
	$(VERBOSE)cp $(REP_DIR)/config/kernel $(KERNEL_BUILD_DIR)/config/config.out


#
# How to pass custom compiler flags to the Pistachio build system
#
# CFLAGS do not work because the variable gets assigned within the build
# system:
#
#   Mk/Makeconf:  'CFLAGS = -ffreestanding $(CCFLAGS)'
#
# However, all flags specified in 'CCFLAGS' will end up in 'CFLAGS' and
# 'CCFLAGS' is never assigned - only extended via '+='. Because of this
# extension mechanism, we have to pass custom flags as environment variables
# rather than make arguments. If we specified 'CCFLAGS' as make argument, the
# '+=' mechanism would not work, leaving out some important flags assigned by
# the Pistachio build system.
#
# Because the Pistachio build system invokes the assembler via the GCC
# front end, we can pass 'CC_MARCH' as 'ASMFLAGS'. The linker, however, is
# invoked directly (using 'ld' rather than 'gcc'). Hence, we need to pass
# real linker flags (e.g., '-melf_i386' rather than -m32) to 'LDFLAGS'.
#
# The Pistachio build system has some built-in heuristics about where to find
# libgcc. Unfortunately, this heuristics break when using a multilib tool chain
# that uses -m64 by default. Hence, we need to supply the build system with the
# correct location via the 'GCCINSTALLDIR' and 'LIBGCCINC' variables.
#

$(KERNEL_BUILD_DIR)/config/.config: $(KERNEL_BUILD_DIR)/Makefile
	$(VERBOSE_MK)CCFLAGS="$(CC_MARCH)" MAKEFLAGS= \
	             $(MAKE) $(VERBOSE_DIR) -C $(KERNEL_BUILD_DIR) batchconfig \
	                     GCCINSTALLDIR=$(LIBGCC_DIR)

$(KERNEL): $(KERNEL_BUILD_DIR)/config/.config
	$(VERBOSE_MK)CCFLAGS="$(CC_MARCH)" LDFLAGS="$(LD_MARCH)" ASMFLAGS="$(CC_MARCH)" MAKEFLAGS= \
	             $(MAKE) $(VERBOSE_DIR) -C $(KERNEL_BUILD_DIR) \
	                     TOOLPREFIX=$(CROSS_DEV_PREFIX) \
	                     GCCINSTALLDIR=$(LIBGCC_DIR) \
	                     LIBGCCINC=$(GCCINC_DIR)
	$(VERBOSE)ln -sf $@ $(BUILD_BASE_DIR)/bin/$(TARGET)
	$(VERBOSE)touch $@

clean cleanall:
	$(VERBOSE)rm -rf $(KERNEL_BUILD_DIR)
