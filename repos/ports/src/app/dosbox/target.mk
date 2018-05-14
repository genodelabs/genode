REQUIRES = x86

TARGET := dosbox

DOSBOX_DIR := $(call select_from_ports,dosbox)/src/app/dosbox

SRC_CC_cpu      = $(notdir $(wildcard $(DOSBOX_DIR)/src/cpu/*.cpp))
SRC_CC_debug    = $(notdir $(wildcard $(DOSBOX_DIR)/src/debug/*.cpp))
FILTER_OUT_dos  = cdrom_aspi_win32.cpp cdrom_ioctl_linux.cpp cdrom_ioctl_os2.cpp \
                  cdrom_ioctl_win32.cpp
SRC_CC_dos      = $(filter-out $(FILTER_OUT_dos),$(notdir $(wildcard $(DOSBOX_DIR)/src/dos/*.cpp)))
SRC_CC_fpu      = $(notdir $(DOSBOX_DIR)/src/fpu/fpu.cpp)
SRC_CC_gui      = $(notdir $(wildcard $(DOSBOX_DIR)/src/gui/*.cpp))
FILTER_OUT_hw   = opl.cpp
SRC_CC_hw       = $(filter-out $(FILTER_OUT_hw),$(notdir $(wildcard $(DOSBOX_DIR)/src/hardware/*.cpp)))
SRC_CC_hw_ser   = $(notdir $(wildcard $(DOSBOX_DIR)/src/hardware/serialport/*.cpp))
SRC_CC_ints     = $(notdir $(wildcard $(DOSBOX_DIR)/src/ints/*.cpp))
SRC_CC_misc     = $(notdir $(wildcard $(DOSBOX_DIR)/src/misc/*.cpp))
SRC_CC_shell    = $(notdir $(wildcard $(DOSBOX_DIR)/src/shell/*.cpp))

SRC_CC          = $(notdir $(DOSBOX_DIR)/src/dosbox.cpp)
SRC_CC         += $(SRC_CC_cpu) $(SRC_CC_debug) $(SRC_CC_dos) $(SRC_CC_fpu) $(SRC_CC_gui) \
                  $(SRC_CC_hw) $(SRC_CC_hw_ser) $(SRC_CC_ints) $(SRC_CC_ints) $(SRC_CC_misc) \
                  $(SRC_CC_shell)

vpath %.cpp $(DOSBOX_DIR)/src
vpath %.cpp $(DOSBOX_DIR)/src/cpu
vpath %.cpp $(DOSBOX_DIR)/src/debug
vpath %.cpp $(DOSBOX_DIR)/src/dos
vpath %.cpp $(DOSBOX_DIR)/src/fpu
vpath %.cpp $(DOSBOX_DIR)/src/gui
vpath %.cpp $(DOSBOX_DIR)/src/hardware
vpath %.cpp $(DOSBOX_DIR)/src/hardware/serialport
vpath %.cpp $(DOSBOX_DIR)/src/ints
vpath %.cpp $(DOSBOX_DIR)/src/misc
vpath %.cpp $(DOSBOX_DIR)/src/shell

INC_DIR += $(PRG_DIR)
INC_DIR += $(DOSBOX_DIR)/include
INC_DIR += $(addprefix $(DOSBOX_DIR)/src, cpu debug dos fpu gui hardware hardware/serialport \
           ints misc shell)

CC_OPT  = -DHAVE_CONFIG_H -D_GNU_SOURCE=1 -D_REENTRANT
ifeq ($(filter-out $(SPECS),x86_32),)
INC_DIR += $(PRG_DIR)/spec/x86_32
CC_OPT  += -DC_TARGETCPU=X86
else ifeq ($(filter-out $(SPECS),x86_64),)
INC_DIR += $(PRG_DIR)/spec/x86_64
CC_OPT  += -DC_TARGETCPU=X86_64
endif

CC_WARN  = -Wall
CC_WARN += -Wno-unused-variable -Wno-unused-function -Wno-switch -Wno-unused-value \
           -Wno-unused-but-set-variable -Wno-format -Wno-maybe-uninitialized \
           -Wno-sign-compare -Wno-narrowing -Wno-missing-braces -Wno-array-bounds \
           -Wno-parentheses

LIBS += libpng libc sdl sdlmain sdl_net stdcxx zlib

CC_CXX_WARN_STRICT =
