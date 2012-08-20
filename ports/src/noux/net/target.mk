TARGET   = noux_net
LIBS     = cxx env server process signal lwip thread alarm

LIBS    += libc libc_lwip

SRC_CC   = main.cc net.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc $(PRG_DIR)/..
vpath net.cc  $(PRG_DIR)
