TARGET  = ssh_terminal
SRC_CC  = main.cc
SRC_CC += server.cc
SRC_CC += ssh_callbacks.cc
SRC_CC += util.cc
LIBS    = base libc libssh libc_pipe

CC_CXX_WARN_STRICT =
