include $(REP_DIR)/ports/libvorbis.inc
LIBVORBIS_DIR = $(REP_DIR)/contrib/$(LIBVORBIS)/lib

# filter the test programs
FILTER_OUT = barmel.c tone.c psytune.c

SRC_C = $(filter-out $(FILTER_OUT), $(notdir $(wildcard $(LIBVORBIS_DIR)/*.c)))

vpath %.c $(LIBVORBIS_DIR)

INC_DIR += $(LIBVORBIS_DIR)

#CFLAGS +=  -ffast-math -D_REENTRANT -fsigned-char -Wdeclaration-after-statement  -DUSE_MEMORY_H -Wnounused-variable

LIBS += libc libm libogg

SHARED_LIB = yes
