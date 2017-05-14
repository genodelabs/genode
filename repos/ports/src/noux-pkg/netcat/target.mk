TARGET = netcat

LIBS = libc_resolv

include $(call select_from_repositories,mk/noux.mk)
