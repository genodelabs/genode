JENT_CONTRIB_DIR := $(call select_from_ports,jitterentropy)

INC_DIR += $(JENT_CONTRIB_DIR)/include/jitterentropy \
           $(REP_DIR)/src/lib/jitterentropy

SRC_C  = jitterentropy-base.c
SRC_CC = jitterentropy-base-genode.cc

CC_OPT += -DJITTERENTROPY_GENODE

override CC_OLEVEL := -O0

vpath %.c  $(JENT_CONTRIB_DIR)/src/lib/jitterentropy
vpath %.cc $(REP_DIR)/src/lib/jitterentropy
