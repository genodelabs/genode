TARGET   = test-lxip_server
LIBS     = lxip base net
SRC_CC   = main.cc

IP_DIR = $(REP_DIR)/src/test/ip_raw

INC_DIR += $(IP_DIR)/include

vpath %.cc $(IP_DIR)/server

CC_CXX_WARN_STRICT =
