TARGET = netperf

CONTRIB_DIR = $(REP_DIR)/contrib/netperf

LIBS   += base libc libm libc-resolv libc-net libc-nameser libc-isc
# plug-in to libc
LIBS   += libc_log libc_lwip_nic_dhcp config_args

SRC_C   = netserver.c netlib.c netsh.c nettest_bsd.c
# omni test
SRC_C  += nettest_omni.c net_uuid.c
SRC_C  += netsys_none.c netsec_none.c netdrv_none.c netrt_none.c netslot_none.c netcpu_none.c

INC_DIR += $(PRG_DIR)
CC_OPT  += -DHAVE_CONFIG_H -DGENODE_BUILD

CC_WARN = -Wall -Wno-unused

vpath %.c $(CONTRIB_DIR)/src
