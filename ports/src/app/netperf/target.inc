
CONTRIB_DIR = $(REP_DIR)/contrib/netperf

LIBS   += base libc libm libc-resolv libc-net libc-nameser libc-isc
# plug-in to libc
LIBS   += config_args

SRC_C   = netserver.c netlib.c netsh.c nettest_bsd.c dscp.c
# omni test
SRC_C  += nettest_omni.c net_uuid.c
SRC_C  += netsys_none.c netsec_none.c netdrv_none.c netrt_none.c netslot_none.c netcpu_none.c

SRC_CC += timer.cc

INC_DIR += $(PRG_DIR)/..
CC_OPT  += -DHAVE_CONFIG_H -DGENODE_BUILD

CC_WARN = -Wall -Wno-unused

vpath %.c $(CONTRIB_DIR)/src
vpath timer.cc $(PRG_DIR)/..

# vi: set ft=make :
