include $(REP_DIR)/etc/board.conf

TARGET   := foc-$(BOARD)
LIBS     := core-foc
CORE_OBJ := core-foc-$(BOARD).o

include $(BASE_DIR)/src/core/target.inc
