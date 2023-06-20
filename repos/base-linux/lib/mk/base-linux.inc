#
# The content of this file is used for both native Genode as well as hybrid
# Linux/Genode programs. Hence, it must be void of any thread-related code.
#

include $(BASE_DIR)/lib/mk/base.inc

SRC_CC += platform.cc
SRC_CC += capability_slab.cc

LIBS += syscall-linux
