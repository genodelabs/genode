CONFIGURE_FLAGS = --without-included-regex

LIBS += pcre

include $(call select_from_repositories,mk/noux.mk)
