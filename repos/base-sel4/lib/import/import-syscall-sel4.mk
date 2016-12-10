SEL4_INCLUDE_DIR := $(LIB_CACHE_DIR)/syscall-sel4/include

#
# Access kernel-interface headers that were installed when building the
# syscall-sel4 library.
#
INC_DIR += $(SEL4_INCLUDE_DIR)

#
# Access to other sel4-specific headers such as 'autoconf.h'.
#
INC_DIR += $(SEL4_INCLUDE_DIR)/sel4

# required for seL4_DebugPutChar
CC_OPT += -DSEL4_DEBUG_KERNEL -DDEBUG
