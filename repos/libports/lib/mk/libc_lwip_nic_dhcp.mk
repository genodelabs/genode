SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip_nic_dhcp

LIBS += lwip_legacy libc libc_lwip

CC_CXX_WARN_STRICT =
