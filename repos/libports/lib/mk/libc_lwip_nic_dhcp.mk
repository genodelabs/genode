SRC_CC = init.cc plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip_nic_dhcp

LIBS += lwip libc libc_lwip
