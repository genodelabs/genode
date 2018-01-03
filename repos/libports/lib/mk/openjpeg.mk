OPENJPEG     = openjpeg-1.4
OPENJPEG_DIR = $(call select_from_ports,openjpeg)/src/lib/openjpeg/libopenjpeg
LIBS        += libc libm
INC_DIR     += $(OPENJPEG_DIR) $(REP_DIR)/include/openjpeg

# incorporate all '*.c' files except those that are not part of the library
FILTER_OUT = t1_generate_luts.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(OPENJPEG_DIR)/*.c)))

# disable warning noise for contrib code
CC_WARN = -Wno-unused-but-set-variable

vpath %.c $(OPENJPEG_DIR)


#
# Provide 'opj_config.h' file normally created by the configure process
#
opj_config.h:
	$(VERBOSE)echo "#define PACKAGE_VERSION \"1.4.0\"" > $@

$(SRC_C:.c=.o): opj_config.h

clean: clean_opj_config_h


#
# 'malloc.h' is deprecated, yet still included by libopenjpeg, so that we have
# to provide a dummy implementation here.
#
malloc.h:
	$(VERBOSE)echo "#include <stdlib.h>" > $@
	$(VERBOSE)echo "#undef HAVE_MEMALIGN" >> $@

$(SRC_C:.c=.o): malloc.h

clean: clean_malloc_h


clean_malloc_h clean_opj_config_h:
	$(VERBOSE)rm -f malloc.h


SHARED_LIB = yes

CC_CXX_WARN_STRICT =
