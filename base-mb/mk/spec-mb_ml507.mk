SPECS += 32bit mb_timer

STARTUP_LIB ?= startup

PRG_LIBS += $(STARTUP_LIB)

include $(call select_from_repositories,mk/spec-32bit.mk)
