PLATFORM_DIR = $(REP_DIR)/src/kernel/platforms/petalogix_s3adsp1800_mmu

INC_DIR += $(PLATFORM_DIR)/include

SRC_S += atomic.s
vpath atomic.s $(PLATFORM_DIR)
