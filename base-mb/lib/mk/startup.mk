PLATFORM     = petalogix_s3adsp1800_mmu
KERNEL_DIR   = $(REP_DIR)/src/kernel
PLATFORM_DIR = $(KERNEL_DIR)/platforms/$(PLATFORM)

LIBS     = cxx lock
SRC_S    = crt0.s
SRC_CC  += _main.cc
INC_DIR += $(REP_DIR)/src/platform
INC_DIR += $(BASE_DIR)/src/platform
INC_DIR += $(PLATFORM_DIR)/include

vpath crt0.s      $(PLATFORM_DIR)
vpath _main.cc    $(dir $(call select_from_repositories,src/platform/_main.cc))
