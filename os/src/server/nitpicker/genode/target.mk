TARGET   = nitpicker
LIBS     = base blit
SRC_CC   = main.cc \
           view_stack.cc \
           view.cc \
           user_state.cc
SRC_BIN  = default.tff

INC_DIR  = $(PRG_DIR)/../include \
           $(PRG_DIR)/../data

vpath %.cc $(PRG_DIR)/../common
vpath %    $(PRG_DIR)/../data

