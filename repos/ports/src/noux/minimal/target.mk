TARGET   = noux
LIBS     = alarm config vfs
SRC_CC   = main.cc dummy_net.cc
INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc      $(PRG_DIR)/..
vpath dummy_net.cc $(PRG_DIR)
