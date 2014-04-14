TARGET   = test-ping_client_libc_lwip
LIBS     = base libc lwip libc_lwip_nic_dhcp libc_vfs config_args
SRC_CC   = main.cc pingpong.cc

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..
