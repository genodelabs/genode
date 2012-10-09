TARGET  = terminal
LIBS    = cxx env server signal
SRC_CC  = main.cc
SRC_BIN = $(notdir $(wildcard $(PRG_DIR)/*.tff))
