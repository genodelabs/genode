TARGET   = terminal_mux
SRC_CC   = main.cc ncurses.cc
LIBS     = libc libc_terminal ncurses
INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT =
