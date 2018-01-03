TARGET = netserver_lwip

LIBS  += libc_lwip_nic_dhcp

include $(PRG_DIR)/../target.inc

CC_CXX_WARN_STRICT =
