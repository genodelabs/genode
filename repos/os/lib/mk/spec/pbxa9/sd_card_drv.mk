SRC_CC  += spec/pbxa9/driver.cc
LIBS    += base

vpath %.cc $(REP_DIR)/src/drivers/sd_card

include $(REP_DIR)/lib/import/import-sd_card_drv.mk
