MUPDF_DIR := $(call select_from_ports,mupdf)/src/lib/mupdf
TARGET    := mupdf
SRC_C     := pdfapp.c
SRC_CC    := main.cc
LIBS      := libc mupdf
INC_DIR   += $(MUPDF_DIR)/apps

vpath pdfapp.c $(MUPDF_DIR)/apps
