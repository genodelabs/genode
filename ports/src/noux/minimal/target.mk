TARGET   = noux
LIBS     = cxx env server process signal thread alarm
SRC_CC   = main.cc dummy_net.cc
INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc      $(PRG_DIR)/..
vpath dummy_net.cc $(PRG_DIR)
