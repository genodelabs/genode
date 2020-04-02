PORT_DIR := $(call select_from_ports,libc)

# Depot hack
ifneq ($(PORT_DIR),)
LIBM_PORT_DIR = $(PORT_DIR)/
endif

LIBM_SRC_DIR := $(LIBM_PORT_DIR)src/lib/openlibm

SHARED_LIB = yes
LIBS += libc

CC_C_OPT += -fno-gnu89-inline
CC_C_OPT += -std=c99 -DASSEMBLER -D__BSD_VISIBLE -Wno-implicit-function-declaration

CC_OPT += -include sys/cdefs.h

INC_DIR += $(REP_DIR)/include/openlibm
INC_DIR += $(LIBM_SRC_DIR)
INC_DIR += $(LIBM_SRC_DIR)/src
INC_DIR += $(LIBM_PORT_DIR)include/openlibm
INC_DIR += $(LIBM_SRC_DIR)/include

SUBDIRS = src $(ARCH) bsdsrc
# Add ld80 directory on x86 and x64
ifneq ($(filter $(ARCH),i387 amd64),)
SUBDIRS += ld80
else
ifneq ($(filter $(ARCH),aarch64),)
SUBDIRS += ld128
else
endif
endif

define INC_template
TEST=test
override CUR_SRCS = $(1)_SRCS
-include $(LIBM_SRC_DIR)/$(1)/Make.files
SRCS += $$(addprefix $(1)/,$$($(1)_SRCS))
endef

$(foreach dir,$(SUBDIRS),$(eval $(call INC_template,$(dir))))

DUPLICATE_NAMES = $(filter $(patsubst %.S,%,$($(ARCH)_SRCS)),$(patsubst %.c,%,$(src_SRCS)))
DUPLICATE_SRCS = $(addsuffix .c,$(DUPLICATE_NAMES))

CLEAN_SRCS = $(filter-out $(DUPLICATE_SRCS),$(notdir $(SRCS)))

SRC_C += $(filter %.c,$(CLEAN_SRCS))
SRC_S += $(filter %.S,$(CLEAN_SRCS))

$(foreach dir,$(SUBDIRS),$(eval vpath %.c $(LIBM_SRC_DIR)/$(dir)))
$(foreach dir,$(SUBDIRS),$(eval vpath %.S $(LIBM_SRC_DIR)/$(dir)))

$(foreach dir,$(SUBDIRS),$(eval INC_DIR += $(LIBM_SRC_DIR)/$(dir)))
