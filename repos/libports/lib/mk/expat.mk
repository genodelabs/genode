include $(select_from_repositories,lib/import/import-expat.mk)

EXPAT_DIR := $(call select_from_ports,expat)/src/lib/expat/contrib
LIBS      += libc

SRC_C = xmlparse.c xmlrole.c xmltok.c

CC_OPT += -DHAVE_MEMMOVE -DXML_NS=1 -DXML_DTD=1 -DXML_CONTEXT_BYTES=1024

vpath %.c $(EXPAT_DIR)/lib

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
