LIBSSH_DIR := $(call select_from_ports,libssh)

REP_INC_DIR += include/libssh
INC_DIR     += $(LIBSSH_DIR)/include
