SPECS       += 64bit
REP_INC_DIR += include/spec/riscv

# force soft-float for binutiles
AS_OPT += -march rv64imac -mabi=lp64

include $(BASE_DIR)/mk/spec/64bit.mk

