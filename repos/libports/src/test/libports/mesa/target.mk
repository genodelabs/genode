TARGET = test-mesa
LIBS   = libc mesa mesa-egl
LIBS  += $(addprefix gallium-,aux softpipe failover identity egl)
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..
