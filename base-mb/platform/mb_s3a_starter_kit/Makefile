#
# \brief  Prepare the Xilinx Spartan 3A Starter Kit to run Genode on it
# \author Martin Stein
# \date   2011-05-23
#

HW        = s3a_starter_kit
WORK_DIR  = $(shell pwd)
BIT_FILE  = $(WORK_DIR)/system.bit
VERBOSE  ?= @
REP_DIR   = ../..
MK_DIR    = $(REP_DIR)/platform/mk/

all: configure

clean:
	$(VERBOSE) rm -f $(TMP_FILES)

.PHONY: all clean

include $(MK_DIR)/$(HW).mk
include $(MK_DIR)/microblaze.mk
