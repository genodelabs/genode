INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/rpi
SRC_CC  += spec/rpi/driver.cc

vpath main.cc $(REP_DIR)/src/test/sd_card_bench

include $(REP_DIR)/lib/mk/sd_card.inc
