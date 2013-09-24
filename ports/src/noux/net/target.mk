TARGET   = noux_net
LIBS    += alarm libc libc_lwip_nic_dhcp config

SRC_CC   = main.cc net.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc $(PRG_DIR)/..
vpath net.cc  $(PRG_DIR)
