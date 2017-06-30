INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/x86_64

SRC_CC  += bootstrap/spec/x86_64/platform_muen.cc
SRC_CC  += lib/muen/sinfo.cc
SRC_CC  += hw/spec/64bit/memory_map.cc

SRC_S   += bootstrap/spec/x86_64/crt0.s
SRC_S   += bootstrap/spec/x86_64/crt0_translation_table_muen.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
