BOARD    ?= unknown
TARGET   := core_hw_$(BOARD)
LIBS     := core-hw-$(BOARD)
CORE_OBJ := core-hw-$(BOARD).o

include $(BASE_DIR)/src/core/target.inc
