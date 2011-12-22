TARGET = test-mesa
LIBS   = cxx env mesa mesa-egl
LIBS  += $(addprefix gallium-,aux softpipe failover identity egl)
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..
