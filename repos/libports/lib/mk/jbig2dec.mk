JBIG2DEC     = jbig2dec-0.11
JBIG2DEC_DIR = $(REP_DIR)/contrib/$(JBIG2DEC)
LIBS        += libc libpng zlib
INC_DIR     += $(JBIG2DEC_DIR) $(REP_DIR)/include/jbig2dec

# incorporate all '*.c' files except those that are not part of the library
FILTER_OUT = jbig2dec.c snprintf.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(JBIG2DEC_DIR)/*.c)))

# definitions normally provided by a config.h file
CC_OPT += -DHAVE_STRING_H -DHAVE_STDINT_H

# disable warning noise for contrib code
CC_WARN = -Wno-deprecated-declarations

SHARED_LIB = yes

vpath %.c $(JBIG2DEC_DIR)
