TARGET   = test-lwip_client
LIBS     = lwip base net
SRC_CC   = main.cc

IP_DIR = $(REP_DIR)/src/test/ip_raw

INC_DIR += $(IP_DIR)/include

vpath %.cc $(IP_DIR)/client

CC_CXX_WARN_STRICT =
