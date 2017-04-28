INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/x86_64

SRC_S   += bootstrap/spec/x86_64/crt0.s
SRC_CC  += bootstrap/spec/x86_64/platform.cc
SRC_S   += bootstrap/spec/x86_64/crt0_translation_table.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
