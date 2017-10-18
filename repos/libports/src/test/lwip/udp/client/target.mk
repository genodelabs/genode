TARGET   = test-lwip-udp-client
LIBS     = posix libc_lwip_nic_dhcp libc_lwip lwip
SRC_CC   = main.cc

INC_DIR += $(REP_DIR)/src/lib/lwip/include
