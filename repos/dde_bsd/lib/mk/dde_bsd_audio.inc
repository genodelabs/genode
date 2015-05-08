LIB_DIR     = $(REP_DIR)/src/lib/audio
LIB_INC_DIR = $(LIB_DIR)/include

AUDIO_CONTRIB_DIR := $(call select_from_ports,dde_bsd)/src/lib/audio

INC_DIR += $(LIB_DIR)
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(AUDIO_CONTRIB_DIR)

SRC_CC += bus.cc dummies.cc driver.cc irq.cc mem.cc misc.cc scheduler.cc timer.cc
SRC_C += bsd_emul.c
SRC_S += setjmp.S

CC_OPT += -Wno-unused-but-set-variable

# disable builtins
CC_OPT += -fno-builtin-printf -fno-builtin-snprintf -fno-builtin-vsnprintf \
          -fno-builtin-malloc -fno-builtin-free -fno-builtin-log -fno-builtin-log2

CC_OPT += -D_KERNEL

# enable when debugging
# CC_OPT += -DAUDIO_DEBUG
# CC_OPT += -DAZALIA_DEBUG
# CC_OPT += -DDIAGNOSTIC

# audio interface
SRC_C += dev/audio.c dev/auconv.c

# AC97 codec
SRC_C += dev/ic/ac97.c

# HDA driver
SRC_C += dev/pci/azalia.c dev/pci/azalia_codec.c

# ES1370
SRC_C += dev/pci/eap.c

SRC_C += dev/mulaw.c

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'bsd_emul.h'.
#
GEN_INCLUDES := $(shell grep -rh "^\#include .*" $(AUDIO_CONTRIB_DIR) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)

#
# Filter out original BSD headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(AUDIO_CONTRIB_DIR); find -name "*.h" | sed "s/.\///" | sed "s/.*include\///")
GEN_INCLUDES    := $(filter-out $(NO_GEN_INCLUDES),$(GEN_INCLUDES))

#
# Put BSD headers in 'GEN_INC' dir
#
GEN_INC := $(shell pwd)/include

$(shell mkdir -p $(GEN_INC))

GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))
INC_DIR         += $(GEN_INC)

#
# Make sure to create the header symlinks prior building
#
$(SRC_C:.c=.o) $(SRC_CC:.cc=.o): $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(LIB_INC_DIR)/bsd_emul.h $@

vpath %.c  $(AUDIO_CONTRIB_DIR)
vpath %.c  $(LIB_DIR)
vpath %.cc $(LIB_DIR)

# vi: set ft=make :
