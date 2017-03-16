TARGET   = init
SRC_CC   = main.cc child.cc server.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

# workaround for constness issues
CC_OPT  += -fpermissive
