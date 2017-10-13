include $(call select_from_repositories,mk/spec/nova.mk)

TARGET           = hypervisor
REQUIRES         = x86 nova
NOVA_BUILD_DIR   = $(BUILD_BASE_DIR)/kernel
NOVA_SRC_DIR     = $(call select_from_ports,nova)/src/kernel/nova
SRC_CC           = $(sort $(notdir $(wildcard $(NOVA_SRC_DIR)/src/*.cpp)))
SRC_S            = $(sort $(notdir $(wildcard $(NOVA_SRC_DIR)/src/*.S)))
INC_DIR          = $(NOVA_SRC_DIR)/include
override CC_OLEVEL := -Os
CC_WARN          = -Wall -Wextra -Waggregate-return -Wcast-align -Wcast-qual \
                   -Wconversion -Wdisabled-optimization -Wformat=2 \
                   -Wmissing-format-attribute -Wmissing-noreturn -Wpacked \
                   -Wpointer-arith -Wredundant-decls -Wshadow -Wwrite-strings \
                   -Wabi -Wctor-dtor-privacy -Wno-non-virtual-dtor \
                   -Wold-style-cast -Woverloaded-virtual -Wsign-promo \
                   -Wlogical-op -Wstrict-null-sentinel \
                   -Wstrict-overflow=5 -Wvolatile-register-var
CC_OPT          += -pipe \
                   -fdata-sections -fomit-frame-pointer -freg-struct-return \
                   -freorder-blocks -funit-at-a-time -fno-exceptions -fno-rtti \
                   -fno-stack-protector -fvisibility-inlines-hidden \
                   -fno-asynchronous-unwind-tables -std=gnu++0x 
CC_OPT_PIC      :=
ifeq ($(filter-out $(SPECS),32bit),)
override CC_MARCH = -m32
CC_WARN         += -Wframe-larger-than=96
CC_OPT          += -mpreferred-stack-boundary=2 -mregparm=3
else
ifeq ($(filter-out $(SPECS),64bit),)
override CC_MARCH = -m64
CC_WARN         += -Wframe-larger-than=256
CC_OPT          += -mpreferred-stack-boundary=4 -mcmodel=kernel -mno-red-zone
else
$(error Unsupported environment)
endif
endif

git_version      = $(shell cd $(NOVA_SRC_DIR) && (git rev-parse HEAD 2>/dev/null || echo 0) | cut -c1-7)
CXX_LINK_OPT     = -Wl,--gc-sections -Wl,--warn-common -Wl,-static -Wl,-n -Wl,--defsym=GIT_VER=0x$(call git_version)
LD_TEXT_ADDR     = # 0xc000000000 - when setting this 64bit compile fails because of relocation issues!! 
LD_SCRIPT_STATIC = hypervisor.o

$(TARGET): hypervisor.o

hypervisor.o: $(NOVA_SRC_DIR)/src/hypervisor.ld target.mk
	$(VERBOSE)$(CC) $(INCLUDES) -DCONFIG_KERNEL_MEMORY=32M -MP -MMD -pipe $(CC_MARCH) -xc -E -P $< -o $@

clean cleanall:
	$(VERBOSE)rm -rf $(NOVA_BUILD_DIR)

vpath %.cpp $(NOVA_SRC_DIR)/src
vpath %.S $(NOVA_SRC_DIR)/src
