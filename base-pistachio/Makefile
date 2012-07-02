#
# \brief  Checkout Pistachio and addtional needed tools (kickstart)
# \author Stefan Kalkowski
# \date   2011-07-15
#

VERBOSE     = @
ECHO        = @echo
GIT_URI     = https://github.com/l4ka/pistachio.git
GIT_REV     = 5c1b29b9c77fbd4760f35507da3d2f548f4364bd
CONTRIB_DIR = contrib
PATCHES     = $(shell find patches -name *.patch)

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,git)
$(call check_tool,patch)
$(call check_tool,sed)
$(call check_tool,autoheader)
$(call check_tool,autoconf)

#
# Determine python version to use for CML2
#
PYTHON2 := $(notdir $(lastword $(shell which python2 python2.{4,5,6,7,8})))
ifeq ($(PYTHON2),)
prepare: python_not_installed
python_not_installed:
	$(ECHO) "Error: CML2 Configuration System needs Python 2 to be installed"
	@false;
endif

#
# Print help information by default
#
help::
	$(ECHO)
	$(ECHO) "Check out upstream source code of Pistachio"
	$(ECHO)
	$(ECHO) "The source code will be located at the '$(CONTRIB_DIR)/' directory."
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare - checkout upstream source codes"
	$(ECHO) "clean   - remove upstream source codes"
	$(ECHO)

$(CONTRIB_DIR):
	$(VERBOSE)git clone $(GIT_URI) contrib

prepare: $(CONTRIB_DIR)
	$(VERBOSE)cd $(CONTRIB_DIR); git fetch;  git reset --hard $(GIT_REV)
	@# use GCC front end for as linker for the pistachio user land
	$(VERBOSE)sed -i "/LD=/s/^.*$$/LD=\$$(CC)/" $(CONTRIB_DIR)/user/config.mk.in
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $(CONTRIB_DIR) -p1 < $$i; done
	$(VERBOSE)for i in cmlcompile.py cmlconfigure.py configtrans.py; do \
		sed -i "s/env python/env $(PYTHON2)/" $(CONTRIB_DIR)/contrib/cml2/$$i; done
	$(VERBOSE)cd $(CONTRIB_DIR)/user; autoheader; autoconf;

clean::
	$(VERBOSE)rm -rf $(CONTRIB_DIR)
