include $(call select_from_repositories,mk/noux.mk)

INSTALL_TARGET = install

PATCHES := src/noux-pkg/socat/socat.patch
