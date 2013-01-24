TARGET   = test-ping_client
LIBS     = cxx env libc libc_lwip lwip
SRC_CC   = main.cc ../pingpong.cc

CC_OPT_main += -fpermissive

INC_DIR += $(REP_DIR)/src/lib/lwip/include
