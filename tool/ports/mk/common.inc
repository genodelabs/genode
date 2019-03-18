#
# \brief  Common environment
# \author Norman Feske
# \date   2014-05-27
#

# compound directory where all 3rd-party source codes are installed
CONTRIB_DIR ?= $(GENODE_DIR)/contrib

SHELL   := bash
VERBOSE ?= @
ECHO    := echo -e

# limit destructive git operations (e.g., reset) to local directory
GIT := git --git-dir=.git

BRIGHT_COL  ?= \x1b[01;33m
DARK_COL    ?= \x1b[00;33m
DEFAULT_COL ?= \x1b[0m

MSG_PREFIX_TXT := $(DARK_COL)$(notdir $(PORT:.port=))  $(DEFAULT_COL)
MSG_PREFIX     := $(ECHO) "$(MSG_PREFIX_TXT)"
MSG_DOWNLOAD   := $(MSG_PREFIX)"download "
MSG_APPLY      := $(MSG_PREFIX)"apply "
MSG_UPDATE     := $(MSG_PREFIX)"update "
MSG_INSTALL    := $(MSG_PREFIX)"install "
MSG_GENERATE   := $(MSG_PREFIX)"generate "
MSG_EXTRACT    := $(MSG_PREFIX)"extract "
MSG_GIT        := $(MSG_PREFIX_TXT)git 

# Force stable sorting order
export LC_COLLATE=C

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))
