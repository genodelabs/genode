TARGET = stubby
LIBS  += libc posix libgetdns getdns-gldns libcrypto libyaml

STUBBY_DIR     := $(call select_from_ports,getdns)/src/lib/getdns/stubby
STUBBY_SRC_DIR := $(STUBBY_DIR)/src

SRC_C += stubby.c convert_yaml_to_json.c

INC_DIR += $(PRG_DIR) $(STUBBY_SRC_DIR)
INC_DIR += $(STUBBY_SRC_DIR)/../../src
INC_DIR += $(STUBBY_SRC_DIR)/../../src/util/auxiliary

CC_DEF += -DHAVE_CONFIG_H -DSTUBBYCONFDIR=\"/\" -DRUNSTATEDIR=\"/\"

CC_DEF += -DSTUBBY_PACKAGE=\"stubby\" -DSTUBBY_PACKAGE_STRING=\"0.2.2\" 

vpath %.c $(STUBBY_SRC_DIR)
vpath %.c $(STUBBY_SRC_DIR)/yaml
