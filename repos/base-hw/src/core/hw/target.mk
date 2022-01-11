BOARD    ?= unknown
TARGET   := core-hw-$(BOARD)
LIBS     := core-hw-$(BOARD)
CORE_LIB := core-hw-$(BOARD).a

BUILD_ARTIFACTS := $(CORE_LIB)

include $(BASE_DIR)/src/core/target.inc
