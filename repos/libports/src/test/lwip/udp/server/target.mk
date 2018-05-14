TARGET   = test-lwip-udp-server
LIBS     = libc libc_lwip_nic_dhcp libc_lwip lwip_legacy
SRC_CC   = main.cc

INC_DIR += $(REP_DIR)/src/lib/lwip/include

CC_CXX_WARN_STRICT =
