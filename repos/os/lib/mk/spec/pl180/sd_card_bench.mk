INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/pl180
SRC_CC  += spec/pl180/driver.cc

vpath main.cc $(REP_DIR)/src/test/sd_card_bench

include $(REP_DIR)/lib/mk/sd_card.inc
