TARGET   = test-ping_client_lwip
LIBS     = posix lwip_legacy
SRC_CC   = main.cc pingpong.cc

INC_DIR += $(REP_DIR)/src/lib/lwip/include

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..

CC_CXX_WARN_STRICT =
