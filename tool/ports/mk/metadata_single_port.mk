#
# \brief  Retrieve version information of port source
# \author Christian Helmuth
# \date   2023-01-20
#

PORTS_TOOL_DIR ?= $(GENODE_DIR)/tool/ports

include $(GENODE_DIR)/tool/ports/mk/front_end.inc
include $(GENODE_DIR)/tool/ports/mk/check_port_arg.inc

#
# Include definitions provided by the port description file
#
include $(PORT)

.NOTPARALLEL:

#
# Assertion for the presence of a LICENSE and VERSION declarations in the port
# description
#
ifeq ($(LICENSE),)
$(TARGET): license_undefined
license_undefined:
	@$(ECHO) "Error: License undefined"; false
endif

ifeq ($(VERSION),)
$(TARGET): version_undefined
version_undefined:
	@$(ECHO) "Error: Version undefined"; false
endif

info:
	@$(ECHO) "PORT:     $(PORT_NAME)"
	@$(ECHO) "LICENSE:  $(LICENSE)"
	@$(ECHO) "VERSION:  $(VERSION)"

%.file:
	@$(ECHO) "SOURCE:   $(URL($*))$(if $(VERSION($*)), VERSION $(VERSION($*)),) ($*)"

%.archive:
	@$(ECHO) "SOURCE:   $(URL($*))$(if $(VERSION($*)), VERSION $(VERSION($*)),) ($*)"

%.git:
	@$(ECHO) "SOURCE:   $(URL($*)) git $(REV($*)) ($*)"

%.svn:
	@$(ECHO) "SOURCE:   $(URL($*)) svn $(REV($*)) ($*)"

$(DOWNLOADS): info

$(TARGET): $(DOWNLOADS)
