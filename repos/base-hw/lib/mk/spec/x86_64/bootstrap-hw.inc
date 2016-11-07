INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/x86_64

SRC_S   += bootstrap/spec/x86_64/crt0.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
