TARGET   = noux
LIBS     = alarm vfs
SRC_CC   = main.cc syscall.cc dummy_net.cc construct.cc
INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc      $(PRG_DIR)/..
vpath syscall.cc   $(PRG_DIR)/..
vpath dummy_net.cc $(PRG_DIR)
