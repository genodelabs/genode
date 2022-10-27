include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBICONV_DIR := $(call select_from_ports,libiconv)

ZLIB_DIR   = $(VIRTUALBOX_DIR)/src/libs/zlib-1.2.13
LIBXML_DIR = $(VIRTUALBOX_DIR)/src/libs/libxml2-2.9.4

INC_DIR += $(ZLIB_DIR)
INC_DIR += $(LIBXML_DIR)/include
INC_DIR += $(LIBICONV_DIR)/include/iconv
INC_DIR += $(VBOX_DIR)/Runtime/include

LIBS    += stdcxx

VBOX_CC_OPT += -DLIBXML_THREAD_ENABLED

SRC_C  += buf.c catalog.c chvalid.c debugXML.c dict.c encoding.c error.c entities.c
SRC_C  += globals.c hash.c list.c parser.c parserInternals.c pattern.c
SRC_C  += relaxng.c threads.c tree.c uri.c valid.c HTMLtree.c HTMLparser.c
SRC_C  += SAX.c SAX2.c xmlIO.c xmlmemory.c xmlreader.c xmlregexp.c xmlschemas.c
SRC_C  += xmlschemastypes.c xmlsave.c xmlstring.c xmlunicode.c xpath.c xpointer.c
SRC_C  += xinclude.c

SRC_CC += Runtime/r3/xml.cpp
SRC_CC += Runtime/common/string/ministring.cpp

vpath %.c $(LIBXML_DIR)

CC_CXX_WARN_STRICT =
