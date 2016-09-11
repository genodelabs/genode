GNU_BUILD_MK := $(call select_from_repositories,mk/gnu_build.mk)

LIBS += libc_noux

PKG_DIR ?= $(call select_from_ports,$(PKG))/src/noux-pkg/$(PKG)

include $(GNU_BUILD_MK)

