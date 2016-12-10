L4_CONFIG := $(call select_from_repositories,config/x86_64.user)

L4_INC_TARGETS := amd64/l4/sys amd64/l4f/l4/sys amd64/l4/vcpu

CC_OPT += -Iinclude/amd64

include $(REP_DIR)/lib/mk/spec/x86/syscall-foc.inc
