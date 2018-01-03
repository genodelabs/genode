TARGET  = terminal
SRC_CC  = main.cc
LIBS    = base
SRC_BIN = $(notdir $(wildcard $(PRG_DIR)/*.tff))

CC_CXX_WARN_STRICT =
