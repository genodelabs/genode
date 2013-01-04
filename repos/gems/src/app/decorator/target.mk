TARGET   = decorator
SRC_CC   = main.cc
LIBS     = base config
TFF_DIR  = $(call select_from_repositories,src/app/scout/data)
SRC_BIN  = droidsansb10.tff
INC_DIR += $(PRG_DIR)

vpath %.tff $(TFF_DIR)
