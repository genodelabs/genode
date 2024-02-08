JENT_CONTRIB_DIR := $(call select_from_ports,jitterentropy)

INC_DIR += $(JENT_CONTRIB_DIR)/include/jitterentropy \
           $(REP_DIR)/src/lib/jitterentropy

SRC_C  += jitterentropy-base.c
SRC_C  += jitterentropy-gcd.c
SRC_C  += jitterentropy-health.c
SRC_C  += jitterentropy-noise.c
SRC_C  += jitterentropy-sha3.c
SRC_CC += jitterentropy-base-genode.cc

override CC_OLEVEL := -O0

vpath %.c  $(JENT_CONTRIB_DIR)/src/lib/jitterentropy/src
vpath %.cc $(REP_DIR)/src/lib/jitterentropy
