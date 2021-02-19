include $(REP_DIR)/etc/board.conf

TARGET   := foc-$(BOARD)
LIBS     := core-foc
CORE_LIB := core-foc-$(BOARD).a

include $(BASE_DIR)/src/core/target.inc
