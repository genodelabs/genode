INCLUDE_SUB_DIRS := platform_session \
                    platform_device \
                    spec/arm_64/platform_session \
                    spec/arm_64/platform_device \
                    spec/arm/platform_session \
                    spec/arm/platform_device \
                    spec/imx53/platform_session \
                    spec/rpi/platform \
                    spec/rpi/platform_session \
                    spec/x86/platform_session \
                    spec/x86/platform_device

INCLUDE_DIRS := $(addprefix include/,$(INCLUDE_SUB_DIRS))

MIRRORED_FROM_REP_DIR := $(INCLUDE_DIRS)

include $(REP_DIR)/recipes/api/session.inc
