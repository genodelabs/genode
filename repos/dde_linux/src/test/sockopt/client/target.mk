TARGET   = test-sockopt_client
LIBS     = base libc net
SRC_CC   = main.cc

IP_DIR = $(REP_DIR)/src/test/sockopt

vpath %.cc $(IP_DIR)/client

CC_CXX_WARN_STRICT =
