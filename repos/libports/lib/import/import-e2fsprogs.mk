E2FSPROGS_DIR := $(call select_from_ports,e2fsprogs-lib)

INC_DIR += $(E2FSPROGS_DIR)/src/lib/e2fsprogs/lib
INC_DIR += $(E2FSPROGS_DIR)/include/e2fsprogs

INC_DIR += $(REP_DIR)/src/lib/e2fsprogs
INC_DIR += $(REP_DIR)/src/lib/e2fsprogs/lib
