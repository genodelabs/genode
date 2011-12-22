#
# Aggregate all libraries needed to build a gallium-based GL application
#
LIBS = libc libm mesa mesa-egl gallium-aux gallium-egl gallium-softpipe

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/gallium.inc
