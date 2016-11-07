SRC_CC  += core/spec/x86_64/pic.cc
SRC_CC  += bootstrap/spec/x86_64/platform.cc
SRC_S   += bootstrap/spec/x86_64/crt0_translation_table.s

include $(BASE_DIR)/../base-hw/lib/mk/spec/x86_64/bootstrap-hw.inc
