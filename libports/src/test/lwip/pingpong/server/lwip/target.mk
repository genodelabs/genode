TARGET   = test-ping_server_lwip
LIBS     = base libc lwip libc_log config_args
SRC_CC   = main.cc pingpong.cc

CC_OPT += -DLWIP_NATIVE

CC_OPT_main += -fpermissive

INC_DIR += $(REP_DIR)/src/lib/lwip/include

vpath main.cc     $(PRG_DIR)/..
vpath pingpong.cc $(PRG_DIR)/../..
