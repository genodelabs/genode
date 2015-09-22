#
# Make the includes of src/base/internal/ available to the startup lib. This is
# needed because the seL4-specific src/platform/_main_parent_cap.h as included
# by the startup lib depends on the internal/capability_space_sel4.h.
#
INC_DIR += $(REP_DIR)/src/base

include $(BASE_DIR)/lib/mk/startup.inc

vpath crt0.s $(BASE_DIR)/src/lib/startup/spec/x86_32
