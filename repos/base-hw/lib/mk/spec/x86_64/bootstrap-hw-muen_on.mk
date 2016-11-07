REQUIRES = muen

INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64/muen

SRC_CC  += lib/muen/sinfo.cc
SRC_CC  += bootstrap/spec/x86_64/platform_muen.cc
SRC_S   += bootstrap/spec/x86_64/crt0_translation_table_muen.s

include $(BASE_DIR)/../base-hw/lib/mk/spec/x86_64/bootstrap-hw.inc
