#
# \brief  Download and setup OSS sources
# \author Sebastian Sumpf
# \date   2012-08-30
#

VERBOSE ?= @
ECHO     = @echo

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Download integrate OSS sources with Genode"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and integrate OSS source code"
	$(ECHO)

prepare:
	$(VERBOSE)../../tool/ports/prepare_port oss
