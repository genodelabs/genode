VBOX_MACH := $(filter $(SPECS), x86_32 x86_64)

VBOX_MACH_CC_OPT_x86_32  = -DRT_ARCH_X86 -D__X86__
VBOX_MACH_ASM_OPT_x86_32 = -f elf32
VBOX_MACH_CC_OPT_x86_64  = -DRT_ARCH_AMD64 -D__AMD64__
VBOX_MACH_ASM_OPT_x86_64 = -f elf64

ifeq ($(shell which yasm),)
REQUIRES += installation_of_yasm
REQUIRES += installation_of_iasl
endif

VIRTUALBOX_DIR = $(call select_from_ports,virtualbox)/src/app/virtualbox
VIRTUALBOX_SDK_DIR = $(call select_from_ports,virtualbox)/src/app/virtualbox_sdk

VBOX_DIR       = $(VIRTUALBOX_DIR)/src/VBox

VBOX_CC_OPT += -DIN_RING3 -DVBOX -DVBOX_OSE \
               -D_FILE_OFFSET_BITS=64 -DLOG_ENABLED

VBOX_CC_OPT += $(VBOX_MACH_CC_OPT_$(VBOX_MACH))

VBOX_CC_OPT += -DVBOX_WITH_64_BITS_GUESTS
VBOX_CC_OPT += -DVBOX_WITH_NEW_MSR_CODE

VBOX_CC_OPT += -DIN_SUP_R3 -DIN_VMM_R3

# we use the libc headers from FreeBSD
VBOX_CC_OPT += -DRT_OS_FREEBSD

VBOX_CC_OPT += -DVBOX_WITH_REM
VBOX_CC_OPT += -DVBOX_WITH_HGCM -DVBOX_WITH_HGSMI

VBOX_CC_OPT += -DVBOX_WITHOUT_TESTING_FEATURES

VBOX_CC_OPT += -DVBOX_WITH_VIDEOHWACCEL

VBOX_CC_OPT += -DVBOX_WITH_XPCOM

VBOX_CC_OPT += -DRTLOG_REL_ENABLED -DRT_STRICT -DVBOX_STRICT

VBOX_CC_OPT += -DVBOX_WITH_USB -DVBOX_WITH_VUSB

# Enable Intel Network model E1000
VBOX_CC_OPT += -DVBOX_WITH_E1000

VBOX_CC_OPT += -DVBOX_WITH_AHCI

VIRTUALBOX_VERSION_MAJOR := $(shell cat $(VIRTUALBOX_DIR)/Config.kmk | grep "VBOX_VERSION_MAJOR = " | grep -v "'VBOX_VERSION_MAJOR" | sed "s/^.*= //")
VIRTUALBOX_VERSION_MINOR := $(shell cat $(VIRTUALBOX_DIR)/Config.kmk | grep "VBOX_VERSION_MINOR = " | grep -v "'VBOX_VERSION_MINOR" | sed "s/^.*= //")
VIRTUALBOX_VERSION_BUILD := $(shell cat $(VIRTUALBOX_DIR)/Config.kmk | grep "VBOX_VERSION_BUILD = " | grep -v "'VBOX_VERSION_BUILD" | sed "s/^.*= //")

ifeq ($(VBOX_MACH),x86_32)
VIRTUALBOX_PACKAGE_STRING = GenodeOS_32BIT_GENERIC
else
VIRTUALBOX_PACKAGE_STRING = GenodeOS_64BIT_GENERIC
endif

VBOX_CC_OPT += -DVBOX_VERSION_MAJOR=$(VIRTUALBOX_VERSION_MAJOR) \
               -DVBOX_VERSION_MINOR=$(VIRTUALBOX_VERSION_MINOR) \
               -DVBOX_VERSION_BUILD=$(VIRTUALBOX_VERSION_BUILD) \
               -DVBOX_VERSION_STRING=\"$(VIRTUALBOX_VERSION_MAJOR).$(VIRTUALBOX_VERSION_MINOR).$(VIRTUALBOX_VERSION_BUILD)_OSE\" \
               -DVBOX_PACKAGE_STRING=\"$(VIRTUALBOX_PACKAGE_STRING)\" \
               -DVBOX_API_VERSION_STRING=\"$(VIRTUALBOX_VERSION_MAJOR)_$(VIRTUALBOX_VERSION_MINOR)\" \
               -DIPRT_BLDCFG_VERSION_STRING=\"$(VIRTUALBOX_VERSION_MAJOR).$(VIRTUALBOX_VERSION_MINOR).$(VIRTUALBOX_VERSION_BUILD)_OSE\" \
               -DIPRT_BLDCFG_TARGET=\"genode\" \
               -DIPRT_BLDCFG_TARGET_ARCH=\"$(VBOX_MACH)\"

VBOX_CC_OPT += -DVBOX_WITH_WDDM -DVBOX_WITH_WDDM_W8 -DVBOXWDDM_WITH_VBVA
VBOX_CC_OPT += -DVBOX_WITH_VDMA
VBOX_CC_OPT += -DVBOX_WITH_VMSVGA


VBOX_CC_OPT += -DVBOX_HDD_NO_DYNAMIC_BACKENDS

VBOX_CC_OPT += -DVBOX_WITHOUT_NS_ACCOUNTING

CC_WARN += -Wno-trigraphs

CC_OPT += $(VBOX_CC_OPT)

# VirtualBox expects wchar_t to be a 16-bit type. yasm does not understand this
# flag, therefore it gets added to CC_OPT instead of VBOX_CC_OPT.
CC_OPT += -fshort-wchar

LIBS += libc libm 

INC_DIR += $(REP_DIR)/src/virtualbox/include
INC_DIR += $(REP_DIR)/src/virtualbox/include/xpcom
INC_DIR += $(VIRTUALBOX_DIR)/include
INC_DIR += $(VIRTUALBOX_SDK_DIR)/sdk/bindings/xpcom/include
INC_DIR += $(VBOX_DIR)/Devices/build

vpath %.cpp $(VBOX_DIR)
vpath %.c   $(VBOX_DIR)
vpath %.asm $(VBOX_DIR)

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)yasm $(VBOX_MACH_ASM_OPT_$(VBOX_MACH)) -DASM_FORMAT_ELF -D__YASM__ \
	               $(addprefix -I,$(INC_DIR)) $(VBOX_CC_OPT) -o $@ $<

# vi: set ft=make :
