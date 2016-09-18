TARGET = netserver

NETPERF_DIR := $(call select_from_ports,netperf)/src/app/netperf

LIBS   += posix

SRC_C   = netserver.c netlib.c netsh.c nettest_bsd.c dscp.c
# omni test
SRC_C  += nettest_omni.c net_uuid.c
SRC_C  += netsys_none.c netsec_none.c netdrv_none.c netrt_none.c netslot_none.c netcpu_none.c

SRC_CC += timer.cc

INC_DIR += $(PRG_DIR)
CC_OPT  += -DHAVE_CONFIG_H -DGENODE_BUILD

CC_WARN = -Wall -Wno-unused

CC_CXX_WARN_STRICT =

vpath timer.cc $(PRG_DIR)
vpath %.c      $(NETPERF_DIR)/src

# vi: set ft=make :
