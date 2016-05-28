TARGET = debug-acpica
LIBS   = libc
SRC_CC = os.cc printf.cc

include $(PRG_DIR)/../target.inc

vpath os.cc $(PRG_DIR)/..
