TARGET = netperf

CONTRIB_DIR = $(REP_DIR)/contrib/netperf

LIBS   += base libc libm libc-resolv libc-net libc-nameser libc-isc
# plugins to libc
LIBS   += libc_log libc_lwip_nic_dhcp config_args

SRC_C   = netserver.c netlib.c netsh.c nettest_bsd.c netcpu_none.c dscp.c

INC_DIR += $(PRG_DIR)
CC_OPT  += -DHAVE_CONFIG_H -DGENODE_BUILD

CC_WARN = -Wall -Wno-unused

vpath %.c $(CONTRIB_DIR)/src
