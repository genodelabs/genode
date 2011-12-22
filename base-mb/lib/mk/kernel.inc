KERNEL_DIR = $(REP_DIR)/src/kernel

INC_DIR += $(KERNEL_DIR)/include
INC_DIR += $(REP_DIR)/src/core/include


##
## Platform-specific kernel parts
##

PLATFORM = petalogix_s3adsp1800_mmu

#
# Basic platform support
#
include $(LIBINC_DIR)/$(PLATFORM)__kernel_support.inc

#
# Enable atomic operations for this platform
#
LIBS += $(PLATFORM)__atomic_operations


##
## Generic kernel parts
##

GENERIC_DIR = $(KERNEL_DIR)/generic

SRC_CC  += kernel.cc
SRC_CC  += scheduler.cc
SRC_CC  += thread.cc
SRC_CC  += blocking.cc
SRC_CC  += syscall_events.cc

vpath kernel.cc         $(GENERIC_DIR)
vpath scheduler.cc      $(GENERIC_DIR)
vpath thread.cc         $(GENERIC_DIR)
vpath blocking.cc       $(GENERIC_DIR)
vpath syscall_events.cc $(GENERIC_DIR)

