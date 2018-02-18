include $(REP_DIR)/lib/import/import-libgetdns.mk

GLDNS_SRC_DIR := $(GETDNS_PORT_DIR)/src/lib/getdns/src/gldns

LIBS += libc libssl

INC_DIR += $(GLDNS_SRC_DIR)/..
INC_DIR += $(REP_DIR)/src/lib/getdns

SRC_C += $(notdir $(wildcard $(GLDNS_SRC_DIR)/*.c))

vpath %.c $(GLDNS_SRC_DIR)
