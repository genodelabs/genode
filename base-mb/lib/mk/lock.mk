PLATFORM = petalogix_s3adsp1800_mmu
LIBS     = thread_context $(PLATFORM)__atomic_operations
SRC_CC   = lock.cc
INC_DIR += $(REP_DIR)/src/base/lock

vpath lock.cc  $(BASE_DIR)/src/base/lock
