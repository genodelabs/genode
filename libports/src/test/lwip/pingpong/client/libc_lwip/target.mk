TARGET   = test-ping_client_libc_lwip
LIBS     = base libc libc_lwip lwip libc_lwip_nic_dhcp libc_log config_args
SRC_CC   = main.cc pingpong.cc

CC_OPT_main += -fpermissive

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..
