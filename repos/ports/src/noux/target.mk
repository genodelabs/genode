TARGET   = noux
LIBS     = base alarm vfs
SRC_CC   = main.cc syscall.cc
INC_DIR += $(PRG_DIR)

vpath %.cc   $(PRG_DIR)

CC_CXX_WARN_STRICT =
