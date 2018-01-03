JPEG     = jpeg-7
JPEG_DIR = $(call select_from_ports,jpeg)/src/lib/jpeg
LIBS    += libc

# use our customized 'jconfig.h' file
INC_DIR += $(REP_DIR)/include/jpeg

# add local jpeg headers to include search path
INC_DIR += $(JPEG_DIR)

SRC_C = \
	jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c \
	jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c \
	jcparam.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
	jdarith.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
	jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdpostct.c \
	jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c \
	jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c jutils.c jmemmgr.c \
	jmemnobs.c

# prevent warnings about the word 'main' used as variable name
CC_OPT_jdmainct += -Wno-main
CC_OPT_jcmainct += -Wno-main

vpath %.c $(JPEG_DIR)

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
