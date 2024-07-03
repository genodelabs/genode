REP_INC_DIR += src/bootstrap/spec/x86_64

SRC_S   += bootstrap/spec/x86_64/crt0.s
SRC_CC  += bootstrap/spec/x86_64/platform.cc
SRC_S   += bootstrap/spec/x86_64/crt0_translation_table.s

ARCH_WIDTH_PATH := spec/64bit

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
