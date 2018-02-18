include $(REP_DIR)/lib/import/import-libgetdns.mk

GETDNS_SRC_DIR := $(GETDNS_PORT_DIR)/src/lib/getdns/src

LIBS += libc libcrypto libssl libyaml

INC_DIR += $(GETDNS_SRC_DIR)
INC_DIR += $(GETDNS_SRC_DIR)/../stubby/src
INC_DIR += $(GETDNS_SRC_DIR)/util/auxiliary
INC_DIR += $(REP_DIR)/src/lib/getdns

SRC_C += \
	const-info.c convert.c dict.c dnssec.c general.c \
	list.c request-internal.c platform.c pubkey-pinning.c rr-dict.c \
	rr-iter.c server.c stub.c sync.c ub_loop.c util-internal.c \
	mdns.c context.c rbtree.c select_eventloop.c version.c \

CC_OPT += -D_BSD_SOURCE -D_DEFAULT_SOURCE

SRC_C += $(notdir $(wildcard $(GETDNS_SRC_DIR)/gldns/*.c))
SRC_C += $(notdir $(wildcard $(GETDNS_SRC_DIR)/jsmn/*.c))
SRC_C += $(notdir $(wildcard $(GETDNS_SRC_DIR)/ssl_dane/*.c))

vpath %.c $(GETDNS_SRC_DIR)
vpath %.c $(GETDNS_SRC_DIR)/extension
vpath %.c $(GETDNS_SRC_DIR)/gldns
vpath %.c $(GETDNS_SRC_DIR)/jsmn
vpath %.c $(GETDNS_SRC_DIR)/ssl_dane
vpath %.c $(GETDNS_SRC_DIR)/util

SHARED_LIB = 1
