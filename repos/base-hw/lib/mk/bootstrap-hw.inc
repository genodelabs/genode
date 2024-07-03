LIBS = cxx

SRC_CC  += bootstrap/init.cc
SRC_CC  += bootstrap/lock.cc
SRC_CC  += bootstrap/log.cc
SRC_CC  += bootstrap/platform.cc
SRC_CC  += bootstrap/thread.cc
SRC_CC  += hw/capability.cc
SRC_CC  += lib/base/allocator_avl.cc
SRC_CC  += lib/base/avl_tree.cc
SRC_CC  += lib/base/elf_binary.cc
SRC_CC  += lib/base/heap.cc
SRC_CC  += lib/base/registry.cc
SRC_CC  += lib/base/log.cc
SRC_CC  += lib/base/output.cc
SRC_CC  += lib/base/raw_output.cc
SRC_CC  += lib/base/slab.cc
SRC_CC  += lib/base/sleep.cc
SRC_CC  += lib/base/sliced_heap.cc

REP_INC_DIR += src/bootstrap
REP_INC_DIR += src/include
REP_INC_DIR += src/core/include # for boot_modules.h only

TMP         := $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
BASE_HW_DIR := $(TMP:%/lib/mk/bootstrap-hw.inc=%)

vpath base/%        $(BASE_HW_DIR)/src
vpath bootstrap/%   $(BASE_HW_DIR)/src
vpath hw/%          $(BASE_HW_DIR)/src/lib
vpath lib/base/%    $(BASE_HW_DIR)/src
vpath lib/base/%    $(BASE_DIR)/src

include $(call select_from_repositories,lib/mk/consts-hw.inc)
