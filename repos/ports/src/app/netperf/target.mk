TARGET = netserver

NETPERF_DIR := $(call select_from_ports,netperf)/src/app/netperf

LIBS   += posix

SRC_C   = netserver.c netlib.c netsh.c nettest_bsd.c dscp.c
# omni test
SRC_C  += nettest_omni.c net_uuid.c
SRC_C  += netsys_none.c netsec_none.c netdrv_none.c netrt_none.c netslot_none.c netcpu_none.c

INC_DIR += $(PRG_DIR)
CC_OPT  += -DHAVE_CONFIG_H

CC_WARN = -Wall -Wno-unused -Wno-maybe-uninitialized -Wno-format-truncation \
          -Wno-stringop-truncation -Wno-stringop-overflow

CC_CXX_WARN_STRICT =

vpath %.c $(NETPERF_DIR)/src

# vi: set ft=make :
