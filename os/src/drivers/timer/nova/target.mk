TARGET   = timer
SRC_CC   = main.cc
REQUIRES = nova x86
LIBS     = cxx server env alarm signal

INC_DIR  = $(PRG_DIR)/../include $(PRG_DIR)/../include_pit

vpath main.cc $(PRG_DIR)/..
