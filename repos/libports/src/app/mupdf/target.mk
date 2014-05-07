MUPDF_DIR = $(REP_DIR)/contrib/mupdf-0.9
TARGET    = mupdf
SRC_C     = pdfapp.c
SRC_CC    = main.cc
LIBS      = libc mupdf
INC_DIR  += $(MUPDF_DIR)/apps

vpath pdfapp.c $(MUPDF_DIR)/apps
