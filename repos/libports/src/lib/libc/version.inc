#
# Provide version information to sysctl
#
GENODE_ROOT_DIR = $(REP_DIR)/../..

GENODE_OSRELEASE := $(shell cat $(GENODE_ROOT_DIR)/VERSION)

GENODE_OSREV := $(shell which git > /dev/null && git rev-list --count HEAD)

GENODE_VERSION := $(shell \
		which git > /dev/null \
		&& git -C $(GENODE_ROOT_DIR) describe 2> /dev/null \
		&& exit 0 \
	|| \
		test -r $(GENODE_ROOT_DIR)/VERSION \
		&& cat $(GENODE_ROOT_DIR)/VERSION \
		&& exit 0 \
	|| \
		echo "<unknown version>")

CC_OPT += -DGENODE_OSRELEASE="\"$(GENODE_OSRELEASE)\""
CC_OPT += -DGENODE_OSREV="$(GENODE_OSREV)"
CC_OPT += -DGENODE_VERSION="\"$(GENODE_VERSION)\""

ifeq ($(findstring arm, $(SPECS)), arm)
CC_OPT += -DGENODE_MACHINE="\"arm\""
else
ifeq ($(findstring riscv, $(SPECS)), riscv)
CC_OPT += -DGENODE_MACHINE="\"riscv\""
else
ifeq ($(findstring x86_32, $(SPECS)), x86_32)
CC_OPT += -DGENODE_MACHINE="\"i686\""
else
ifeq ($(findstring x86_64, $(SPECS)), x86_64)
CC_OPT += -DGENODE_MACHINE="\"x86_64\""
else
CC_OPT += -DGENODE_MACHINE="\"unknown\""
endif
endif
endif
endif
