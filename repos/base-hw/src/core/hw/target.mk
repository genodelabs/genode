BOARD    ?= unknown
TARGET   := core_hw_$(BOARD)
LIBS     := core-hw-$(BOARD)
CORE_LIB := core-hw-$(BOARD).a

include $(BASE_DIR)/src/core/target.inc
