INCLUDE_SUB_DIRS := platform_session \
                    spec/imx53/platform_session \
                    spec/rpi/platform_session \
                    spec/x86/platform_session \
                    platform_device \
                    spec/x86/platform_device

INCLUDE_DIRS := $(addprefix include/,$(INCLUDE_SUB_DIRS))

MIRRORED_FROM_REP_DIR := $(INCLUDE_DIRS)

include $(REP_DIR)/recipes/api/session.inc
