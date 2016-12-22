TARGET   = test-ping_server_libc_lwip
LIBS     = posix libc_lwip_nic_dhcp libc_lwip lwip config_args
SRC_CC   = main.cc pingpong.cc

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..
