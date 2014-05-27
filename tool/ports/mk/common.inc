#
# \brief  Common environment
# \author Norman Feske
# \date   2014-05-27
#

SHELL   := bash
VERBOSE ?= @
ECHO    := echo -e
HASHSUM := sha1sum

BRIGHT_COL  ?= \033[01;33m
DARK_COL    ?= \033[00;33m
DEFAULT_COL ?= \033[0m

MSG_PREFIX   := $(ECHO) "$(DARK_COL)$(notdir $(PORT:.port=))  $(DEFAULT_COL)"
MSG_DOWNLOAD := $(MSG_PREFIX)"download "
MSG_APPLY    := $(MSG_PREFIX)"apply "
MSG_UPDATE   := $(MSG_PREFIX)"update "
MSG_INSTALL  := $(MSG_PREFIX)"install "
MSG_GENERATE := $(MSG_PREFIX)"generate "
MSG_EXTRACT  := $(MSG_PREFIX)"extract "

