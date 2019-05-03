SRC_CC  += main.cc driver.cc
LIBS    += base
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/sd_card

vpath %.cc $(REP_DIR)/src/drivers/sd_card
vpath %.cc $(PRG_DIR)
