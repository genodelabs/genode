TARGET   = test-sockopt_server
LIBS     = base libc net
SRC_CC   = main.cc

IP_DIR = $(REP_DIR)/src/test/sockopt

vpath %.cc $(IP_DIR)/server

CC_CXX_WARN_STRICT =
