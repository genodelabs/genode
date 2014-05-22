#
# Provide version information to main.cc
#
GENODE_ROOT_DIR = $(BASE_DIR)/../..
GENODE_VERSION := $(shell \
		which git > /dev/null \
		&& git -C $(GENODE_ROOT_DIR) describe --dirty=" <local changes>" 2> /dev/null \
		&& exit 0 \
	|| \
		test -r $(GENODE_ROOT_DIR)/VERSION \
		&& cat $(GENODE_ROOT_DIR)/VERSION \
		&& exit 0 \
	|| \
		echo "<unknown version>")

CC_OPT_version += -DGENODE_VERSION="\"$(GENODE_VERSION)\""

SRC_CC += version.cc

vpath version.cc $(BASE_DIR)/src/core

version.o: force_version_compilation

force_version_compilation:

# vi: set ft=make :
