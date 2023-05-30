DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

#
# Create symbol alias for jiffies, sharing the value of jiffies_64
#
LD_OPT += --defsym=jiffies=jiffies_64

#
# Lx_emul + Lx_kit definitions
#

SRC_CC  += lx_emul/alloc.cc
SRC_CC  += lx_emul/debug.cc
SRC_CC  += lx_emul/init.cc
SRC_CC  += lx_emul/log.cc
SRC_CC  += lx_emul/page_virt.cc
SRC_CC  += lx_emul/task.cc
SRC_CC  += lx_emul/time.cc
SRC_CC  += lx_emul/irq_flags.cc

SRC_C   += lx_emul/clocksource.c
SRC_C   += lx_emul/shadow/fs/exec.c
SRC_C   += lx_emul/shadow/kernel/cpu.c
SRC_C   += lx_emul/shadow/kernel/exit.c
SRC_C   += lx_emul/shadow/kernel/fork.c
SRC_C   += lx_emul/shadow/kernel/irq_work.c
SRC_C   += lx_emul/shadow/kernel/locking/spinlock.c
SRC_C   += lx_emul/shadow/kernel/pid.c
SRC_C   += lx_emul/shadow/kernel/printk/printk.c
SRC_C   += lx_emul/shadow/kernel/rcu/tree.c
SRC_C   += lx_emul/shadow/kernel/sched/cputime.c
SRC_C   += lx_emul/shadow/kernel/sched/core.c
SRC_C   += lx_emul/shadow/kernel/sched/fair.c
SRC_C   += lx_emul/shadow/kernel/sched/sched.c
SRC_C   += lx_emul/shadow/kernel/smp.c
SRC_C   += lx_emul/shadow/kernel/stop_machine.c
SRC_C   += lx_emul/shadow/lib/delay.c
SRC_C   += lx_emul/shadow/mm/percpu.c
SRC_C   += lx_emul/shadow/mm/slab_common.c
SRC_C   += lx_emul/shadow/mm/slub.c
SRC_C   += lx_emul/shadow/mm/vmstat.c
SRC_C   += lx_emul/start.c
SRC_C   += lx_emul/virt_to_page.c

SRC_CC  += lx_kit/console.cc
SRC_CC  += lx_kit/env.cc
SRC_CC  += lx_kit/init.cc
SRC_CC  += lx_kit/memory.cc
SRC_CC  += lx_kit/scheduler.cc
SRC_CC  += lx_kit/task.cc
SRC_CC  += lx_kit/timeout.cc

ifeq ($(filter-out $(SPECS),x86_32),)
LX_ARCH   := x86
GEN_ARCH  := x86
SPEC_ARCH := x86_32

SRC_C += lx_emul/shadow/arch/x86/kernel/irq.c
SRC_C += lx_emul/shadow/arch/x86/kernel/setup_percpu.c
endif

ifeq ($(filter-out $(SPECS),x86_64),)
LX_ARCH   := x86
GEN_ARCH  := x86
SPEC_ARCH := x86_64


SRC_C += lx_emul/shadow/arch/x86/kernel/irq.c
SRC_C += lx_emul/shadow/arch/x86/kernel/setup_percpu.c
endif

ifeq ($(filter-out $(SPECS),arm),)
LX_ARCH   := arm
GEN_ARCH  := arm
SPEC_ARCH := arm
endif

ifeq ($(filter-out $(SPECS),arm_64),)
LX_ARCH   := arm64
GEN_ARCH  := arm
SPEC_ARCH := arm_64
endif

ifneq ($(SHARED_LIB),)
TARGET_SOURCE_LIST := $(TARGET_LIB_DIR)/spec/$(SPEC_ARCH)/source.list
else
TARGET_SOURCE_LIST := $(PRG_DIR)/source.list
endif

SHADOW_INC_DIR      := $(DDE_LINUX_DIR)/src/include/lx_emul/shadow/include
SPEC_SHADOW_INC_DIR := $(DDE_LINUX_DIR)/src/include/lx_emul/shadow/arch/$(LX_ARCH)/include

SRC_C   += lx_emul/spec/$(GEN_ARCH)/irqchip.c
SRC_C   += lx_emul/spec/$(GEN_ARCH)/start.c
SRC_S   += lx_kit/spec/$(SPEC_ARCH)/setjmp.S

INC_DIR += $(DDE_LINUX_DIR)/src/include/spec/$(SPEC_ARCH)
INC_DIR += $(DDE_LINUX_DIR)/src/include/spec/x86
INC_DIR += $(DDE_LINUX_DIR)/src/include
INC_DIR += $(SPEC_SHADOW_INC_DIR)
INC_DIR += $(SHADOW_INC_DIR)

vpath % $(DDE_LINUX_DIR)/src/lib

#
# Linux kernel definitions
#

INC_DIR += $(LX_SRC_DIR)/arch/$(LX_ARCH)/include
INC_DIR += $(LX_GEN_DIR)/arch/$(LX_ARCH)/include/generated
INC_DIR += $(LX_SRC_DIR)/include
INC_DIR += $(LX_GEN_DIR)/include
INC_DIR += $(LX_SRC_DIR)/arch/$(LX_ARCH)/include/uapi
INC_DIR += $(LX_GEN_DIR)/arch/$(LX_ARCH)/include/generated/uapi
INC_DIR += $(LX_SRC_DIR)/include/uapi
INC_DIR += $(LX_GEN_DIR)/include/generated/uapi

ifeq ($(filter-out $(SPECS),arm_v7),)
CC_DEF  += -D__LINUX_ARM_ARCH__=7
endif

ifeq ($(filter-out $(SPECS),arm_v6),)
CC_DEF  += -D__LINUX_ARM_ARCH__=6
endif

ifeq ($(filter-out $(SPECS),arm),)
# needed definitions of some arm assembler macros
CC_DEF += -include asm/unified.h
endif

CC_DEF   += -include $(LX_SRC_DIR)/include/linux/compiler-version.h
CC_DEF   += -include $(LX_SRC_DIR)/include/linux/kconfig.h
CC_DEF   += -include $(LX_SRC_DIR)/include/linux/compiler_types.h
CC_DEF   += -D__KERNEL__ -DCONFIG_CC_HAS_K_CONSTRAINT=1
CC_DEF   += -DKASAN_SHADOW_SCALE_SHIFT=3
CC_C_OPT += -std=gnu11
CC_C_OPT += -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs
CC_C_OPT += -Werror=implicit-function-declaration -Werror=implicit-int
CC_C_OPT += -Werror=return-type -Wno-format-security -Wno-psabi -Wno-sign-compare
CC_C_OPT += -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-address-of-packed-member
CC_C_OPT += -Wframe-larger-than=2048 -Wno-main -Wno-unused-but-set-variable -Wimplicit-fallthrough
CC_C_OPT += -Wno-unused-const-variable -Wdeclaration-after-statement -Wvla
CC_C_OPT += -Wno-pointer-sign -Wcast-function-type -Wno-stringop-truncation -Wno-array-bounds -Wno-stringop-overflow
CC_C_OPT += -Wno-restrict -Wno-maybe-uninitialized -Werror=date-time
CC_C_OPT += -Wno-alloc-size-larger-than -Wimplicit-fallthrough=5 -Werror=date-time
CC_C_OPT += -Werror=incompatible-pointer-types -Werror=designated-init
CC_C_OPT += -Wno-packed-not-aligned -Wno-unused-but-set-variable
CC_C_OPT += -Wno-discarded-qualifiers

# avoid build errors because with CC_OLEVEL = -O0/-Og - not supported by Linux
override CC_OLEVEL := -O2

LX_SRC   = $(shell grep ".*\.c" $(TARGET_SOURCE_LIST))
SRC_S   += $(shell grep ".*\.S" $(TARGET_SOURCE_LIST))
SRC_C   += $(LX_SRC)

vpath %.c $(LX_SRC_DIR)
vpath %.S $(LX_SRC_DIR)
vpath %.S $(LX_GEN_DIR)

CUSTOM_TARGET_DEPS += $(TARGET_SOURCE_LIST)

# Define helper to generate unique KBUILD_MODNAME from filename and the
# last path element as using the file name is not enough for certain
# drivers (e.g., rtlwifi)
define GEN_KBUILD_MODNAME =
$(lastword $(subst /, ,$(dir $(1))))-$(notdir $(1))
endef

# Define per-compilation-unit CC_OPT defines needed by MODULE* macros in Linux
define CC_OPT_LX_RULES =
CC_OPT_$(1) += -DKBUILD_MODFILE='"$(1)"' -DKBUILD_BASENAME='"$(notdir $(1))"' -DKBUILD_MODNAME='"$(call GEN_KBUILD_MODNAME,$(1))"'
endef

$(foreach file,$(LX_SRC),$(eval $(call CC_OPT_LX_RULES,$(file:%.c=%))))

$(eval $(call CC_OPT_LX_RULES,generated_dummies))
$(eval $(call CC_OPT_LX_RULES,dummies))


#
# Generate crc32table.h header
#

crc32table.h: gen_crc32table
	./gen_crc32table > $@

lib/crc32.o: crc32table.h

gen_crc32table: $(LX_SRC_DIR)/lib/gen_crc32table.c
	$(HOST_CC) -I$(LX_GEN_DIR)/include $< -o $@


#
# Force rebuild whenever shadow headers appear or change
#
# Shadow headers are not handled well by the regular dependency-file mechanism
# and ccache.
#
# As new appearing shadow headers (e.g., when switching branches) are not
# covered by .d files, no rebuild is issued for existing object files that
# actually depend on the just appeared header. Specifying all shadow headers
# as global dependencies forces the rebuild of all potentially affected object
# files in such a situation.
#

GLOBAL_DEPS += $(wildcard $(addsuffix /linux/*.h,$(SHADOW_INC_DIR))) \
               $(wildcard $(addsuffix /asm/*.h,$(SHADOW_INC_DIR)))

GLOBAL_DEPS += $(wildcard $(addsuffix /linux/*.h,$(SPEC_SHADOW_INC_DIR))) \
               $(wildcard $(addsuffix /asm/*.h,$(SPEC_SHADOW_INC_DIR)))

