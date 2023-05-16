REQUIRES += x86

VBOX_MACH := $(filter $(SPECS), x86_32 x86_64)

VBOX_MACH_CC_OPT_x86_32  = -DRT_ARCH_X86 -D__X86__
VBOX_MACH_ASM_OPT_x86_32 = -f elf32
VBOX_MACH_CC_OPT_x86_64  = -DRT_ARCH_AMD64 -D__AMD64__
VBOX_MACH_ASM_OPT_x86_64 = -f elf64

ifeq ($(shell which yasm),)
REQUIRES += installation_of_yasm
REQUIRES += installation_of_iasl
endif

VIRTUALBOX_DIR     := $(call select_from_ports,virtualbox6)/src/virtualbox6
VIRTUALBOX_SDK_DIR := $(call select_from_ports,virtualbox6)/src/virtualbox6_sdk

VBOX_DIR    := $(VIRTUALBOX_DIR)/src/VBox

VBOX_CC_OPT += -DIN_RING3 -DVBOX -DVBOX_OSE
VBOX_CC_OPT += -D_FILE_OFFSET_BITS=64 -DLOG_ENABLED

VBOX_CC_OPT += $(VBOX_MACH_CC_OPT_$(VBOX_MACH))

VBOX_CC_OPT += -DUSING_VMM_COMMON_DEFS
VBOX_CC_OPT += -DVBOX_WITH_64_BITS_GUESTS
VBOX_CC_OPT += -DIN_SUP_R3 -DIN_VMM_R3
VBOX_CC_OPT += -DVBOX_WITH_NATIVE_NEM
VBOX_CC_OPT += -DNEM_WIN_WITH_A20

# we use the libc headers from FreeBSD
VBOX_CC_OPT += -DRT_OS_FREEBSD

VBOX_CC_OPT += -DVBOX_WITH_3RD_IEM_STEP
VBOX_CC_OPT += -DVBOX_WITH_HGCM -DVBOX_WITH_HGSMI
VBOX_CC_OPT += -DVBOX_WITHOUT_TESTING_FEATURES
VBOX_CC_OPT += -DVBOX_WITH_VIDEOHWACCEL
VBOX_CC_OPT += -DVBOX_WITH_XPCOM
VBOX_CC_OPT += -DVBOX_WITH_PDM_ASYNC_COMPLETION
VBOX_CC_OPT += -DVBOX_WITH_NETSHAPER
VBOX_CC_OPT += -DVBOX_WITH_RESOURCE_USAGE_API
VBOX_CC_OPT += -DVBOX_WITH_GUEST_CONTROL
VBOX_CC_OPT += -DVBOX_WITH_GENERIC_SESSION_WATCHER
VBOX_CC_OPT += -DVBOX_WITH_NAT_SERVICE
VBOX_CC_OPT += -DVBOX_WITH_AUDIO_HDA_ASYNC_IO
VBOX_CC_OPT += -DVBOX_WITH_DRAG_AND_DROP
VBOX_CC_OPT += -DVBOX_WITH_SHARED_CLIPBOARD

include $(REP_DIR)/lib/mk/virtualbox6-debug.inc

VBOX_CC_OPT += -DVBOX_WITH_USB -DVBOX_WITH_VUSB

VBOX_CC_OPT += -DVBOX_WITH_NEW_LPC_DEVICE
VBOX_CC_OPT += -DVBOX_WITH_E1000
VBOX_CC_OPT += -DVBOX_WITH_AHCI
VBOX_CC_OPT += -DVBOX_WITH_MULTI_CORE
VBOX_CC_OPT += -DVBOX_WITH_AUDIO_OSS
VBOX_CC_OPT += -DVBOX_WITH_INTEL_HDA

# required for guest additions 5++
VBOX_CC_OPT += -DVBOX_WITH_GUEST_PROPS

# prevent access to port content at the dependency stage of the build system
ifeq ($(called_from_lib_mk),yes)
include $(VIRTUALBOX_DIR)/Version.kmk
CC_CXX_OPT_STD = -std=gnu++17
endif

# if included from virtualbox6/target.mk provide version but leave CC_CXX_OPT_STD alone
ifeq ($(included_from_target_mk),yes)
include $(VIRTUALBOX_DIR)/Version.kmk
endif

VIRTUALBOX_PACKAGE_STRING_x86_32 = GenodeOS_32BIT_GENERIC
VIRTUALBOX_PACKAGE_STRING_x86_64 = GenodeOS_64BIT_GENERIC

VBOX_CC_OPT += -DVBOX_VERSION_MAJOR=$(VBOX_VERSION_MAJOR) \
               -DVBOX_VERSION_MINOR=$(VBOX_VERSION_MINOR) \
               -DVBOX_VERSION_BUILD=$(VBOX_VERSION_BUILD) \
               -DVBOX_VERSION_STRING=\"$(VBOX_VERSION_MAJOR).$(VBOX_VERSION_MINOR).$(VBOX_VERSION_BUILD)_OSE\" \
               -DVBOX_VERSION_STRING_RAW=\"$(VBOX_VERSION_MAJOR).$(VBOX_VERSION_MINOR).$(VBOX_VERSION_BUILD)\" \
               -DVBOX_PACKAGE_STRING=\"$(VIRTUALBOX_PACKAGE_STRING_$(VBOX_MACH))\" \
               -DVBOX_API_VERSION_STRING=\"$(VBOX_VERSION_MAJOR)_$(VBOX_VERSION_MINOR)\" \
               -DIPRT_BLDCFG_VERSION_STRING=\"$(VBOX_VERSION_MAJOR).$(VBOX_VERSION_MINOR).$(VBOX_VERSION_BUILD)_OSE\" \
               -DIPRT_BLDCFG_TARGET=\"genode\" \
               -DIPRT_BLDCFG_TARGET_ARCH=\"$(VBOX_MACH)\"

VBOX_CC_OPT += -DVBOX_WITH_WDDM -DVBOXWDDM_WITH_VBVA
VBOX_CC_OPT += -DVBOX_WITH_VDMA
VBOX_CC_OPT += -DVBOX_WITH_VMSVGA -DVBOX_WITH_VMSVGA3D -DVMSVGA3D_OPENGL
VBOX_CC_OPT += -DVBOX_HDD_NO_DYNAMIC_BACKENDS
VBOX_CC_OPT += -DVBOX_WITH_SCSI
VBOX_CC_OPT += -DVBOX_WITH_HOSTNETIF_API

# prevent error preprocessor directive in VBox/version.h
VBOX_CC_OPT += -DVBOX_PRIVATE_BUILD_DESC

CC_WARN += -Wno-trigraphs -Werror

CC_OPT += $(VBOX_CC_OPT)

# enforce some global definitions (e.g., include of base/log.h)
CC_OPT += -include global_defs.h

# VirtualBox expects wchar_t to be a 16-bit type. yasm does not understand this
# flag, therefore it gets added to CC_OPT instead of VBOX_CC_OPT.
CC_OPT += -fshort-wchar

LIBS += libc libm libc-mem

INC_DIR += $(REP_DIR)/src/virtualbox6/include
INC_DIR += $(VIRTUALBOX_DIR)/include
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include/xpcom
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include/nsprpub
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include/ipcd
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include/string
INC_DIR += $(VBOX_DIR)/Devices/build

vpath %.cpp $(VBOX_DIR)
vpath %.c   $(VBOX_DIR)
vpath %.asm $(VBOX_DIR)

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)yasm $(VBOX_MACH_ASM_OPT_$(VBOX_MACH)) -DASM_FORMAT_ELF -D__YASM__ \
	               $(addprefix -I,$(INC_DIR)) $(VBOX_CC_OPT) -o $@ $<

# vi: set ft=make :
