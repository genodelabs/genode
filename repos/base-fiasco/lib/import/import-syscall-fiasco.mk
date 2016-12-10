L4_SRC_DIR := $(call select_from_ports,fiasco)/src/kernel/fiasco/fiasco/snapshot

INC_DIR += $(LIB_CACHE_DIR)/syscall-fiasco/include/x86/l4v2 \
           $(LIB_CACHE_DIR)/syscall-fiasco/include/x86 \
           $(LIB_CACHE_DIR)/syscall-fiasco/include \
           $(LIB_CACHE_DIR)/syscall-fiasco/include/l4v2
