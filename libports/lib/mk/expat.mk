include $(REP_DIR)/lib/import/import-expat.mk

EXPAT_DIR = $(REP_DIR)/contrib/$(EXPAT)
LIBS     += libc

SRC_C = xmlparse.c xmlrole.c xmltok.c

CC_OPT += -DHAVE_MEMMOVE -DXML_NS=1 -DXML_DTD=1 -DXML_CONTEXT_BYTES=1024

vpath %.c $(EXPAT_DIR)/lib

SHARED_LIB = yes
