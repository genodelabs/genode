MUPDF_DIR := $(call select_from_ports,mupdf)/src/lib/mupdf
TARGET    := pdf_view
SRC_C     := pdfapp.c
SRC_CC    := main.cc
LIBS      := base libc mupdf
INC_DIR   += $(MUPDF_DIR)/apps

vpath pdfapp.c $(MUPDF_DIR)/apps
