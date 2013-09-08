TARGET   = nitpicker
LIBS     = base blit config server
SRC_CC   = main.cc \
           view_stack.cc \
           view.cc \
           user_state.cc \
           global_keys.cc

SRC_BIN  = default.tff

# enable C++11 support
CC_CXX_OPT += -std=gnu++11
