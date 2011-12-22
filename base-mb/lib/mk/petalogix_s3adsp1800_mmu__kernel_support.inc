##
## Platform
##
PLATFORM = petalogix_s3adsp1800_mmu

#
# Assembly include paths
#
INC_DIR += $(KERNEL_DIR)/platforms/$(PLATFORM)/include

#
# C++ include paths
#
INC_DIR += $(KERNEL_DIR)/include/$(PLATFORM)

#
# Sources
#
PLATFORM_DIR = $(KERNEL_DIR)/platforms/$(PLATFORM)

SRC_CC += platform.cc
SRC_S  += crt0_kernel.s 
SRC_S  += kernel_entry.s
SRC_S  += userland_entry.s

vpath platform.cc         $(PLATFORM_DIR)
vpath crt0_kernel.s       $(PLATFORM_DIR)
vpath kernel_entry.s      $(PLATFORM_DIR)
vpath userland_entry.s    $(PLATFORM_DIR)
