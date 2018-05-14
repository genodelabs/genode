TARGET   = test-ping_client_libc_lwip
LIBS     = posix lwip_legacy libc_lwip_nic_dhcp
SRC_CC   = main.cc pingpong.cc

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..

CC_CXX_WARN_STRICT =
