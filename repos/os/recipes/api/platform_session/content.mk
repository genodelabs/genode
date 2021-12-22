INCLUDE_SUB_DIRS := platform_session \
                    legacy/imx53/platform_session \
                    legacy/rpi/platform \
                    legacy/rpi/platform_session \
                    legacy/x86/platform_session \
                    legacy/x86/platform_device

INCLUDE_DIRS := $(addprefix include/,$(INCLUDE_SUB_DIRS))

MIRRORED_FROM_REP_DIR := $(INCLUDE_DIRS)

include $(REP_DIR)/recipes/api/session.inc
