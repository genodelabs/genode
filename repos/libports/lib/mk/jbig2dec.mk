JBIG2DEC_DIR = $(call select_from_ports,jbig2dec)/src/lib/jbig2dec
LIBS        += libc libpng zlib
INC_DIR     += $(JBIG2DEC_DIR) $(REP_DIR)/include/jbig2dec

# incorporate all '*.c' files except those that are not part of the library
FILTER_OUT = jbig2dec.c snprintf.c pbm2png.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(JBIG2DEC_DIR)/*.c)))

# definitions normally provided by a config.h file
CC_OPT += -DHAVE_STRING_H -DHAVE_STDINT_H

# disable warning noise for contrib code
CC_WARN = -Wno-deprecated-declarations

SHARED_LIB = yes

vpath %.c $(JBIG2DEC_DIR)

CC_CXX_WARN_STRICT =
