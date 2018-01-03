TARGET = lighttpd

include $(REP_DIR)/src/app/lighttpd/target.inc

LIBS += libc libm libc_lwip_nic_dhcp

CC_CXX_WARN_STRICT =
