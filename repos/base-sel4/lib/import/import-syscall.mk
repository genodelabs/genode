#
# Access to kernel-interface headers that were installed in the build directory
# when building the platform library.
#
INC_DIR += $(BUILD_BASE_DIR)/include

#
# Access to other sel4-specific headers such as 'autoconf.h'.
#
REP_INC_DIR += include/sel4

#
# Compile code that uses the system-call bindings without -fPIC. Otherwise,
# the compiler would complain with the following error:
#
# error: inconsistent operand constraints in an ‘asm’
#
# XXX avoid mixing PIC with non-PIC code
#
# The lack of PIC support in the seL4 system bindings ultimately results in
# a mix of PIC and non-PIC code within a single binary. This may become a
# problem when using shared libraries.
#
CC_OPT_PIC =
