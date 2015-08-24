SRC_CC  += main.cc
LIBS    += base config server
INC_DIR += $(REP_DIR)/src/drivers/gpio/rpi

vpath % $(REP_DIR)/src/drivers/gpio/rpi
