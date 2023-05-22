TARGET = less

LIBS   = ncurses

INSTALL_TARGET = install

include $(call select_from_repositories,mk/noux.mk)
