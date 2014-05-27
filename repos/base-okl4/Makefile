#
# \brief  Download, unpack and patch OKL4 source code
# \author Stefan Kalkowski
# \date   2011-05-02
#

VERBOSE ?= @

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the OKL4 base repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and extract the OKL4 source code"
	$(ECHO)

prepare:
	$(VERBOSE)../../tool/ports/prepare_port okl4

clean cleanall:
