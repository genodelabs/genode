TARGET  = terminal
SRC_CC  = main.cc
LIBS    = base timeout
SRC_BIN = $(notdir $(wildcard $(PRG_DIR)/*.tff))
