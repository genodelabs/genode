#
# \brief  Checkout Fiasco.OC and addtional needed tools (sigma0, bootstrap)
# \author Stefan Kalkowski
# \author Sebastian Sumpf
# \date   2011-03-31
#

VERBOSE    ?= @
ECHO        = @echo
SVN_URI     = http://svn.tudos.org/repos/oc/tudos/trunk
GIT_URI     = http://github.com/ssumpf/foc.git
GIT_BRANCH  = r47
CONTRIB_DIR = contrib
GNU_FIND   ?= find

SVN_TARGETS = tools/preprocess \
              kernel/fiasco \
              l4/conf \
              l4/doc \
              l4/mk \
              l4/tool \
              l4/pkg/bootstrap \
              l4/pkg/cxx \
              l4/pkg/drivers-frst \
              l4/pkg/l4sys \
              l4/pkg/l4util \
              l4/pkg/ldscripts \
              l4/pkg/libgcc-pure \
              l4/pkg/libgcc \
              l4/pkg/libsigma0 \
              l4/pkg/sigma0 \
              l4/pkg/uclibc-headers \
              l4/pkg/uclibc-minimal \
              l4/pkg/uclibc \
              l4/pkg/libvcpu

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,git)
$(call check_tool,svn)
$(call check_tool,$(GNU_FIND))

#
# Print help information by default
#
help::

$(CONTRIB_DIR)/.git:
	$(VERBOSE)git clone $(GIT_URI) $(CONTRIB_DIR)

prepare: $(CONTRIB_DIR)/.git
	$(VERBOSE)cd $(CONTRIB_DIR); git fetch origin
	$(VERBOSE)cd $(CONTRIB_DIR); git checkout $(GIT_BRANCH)
	$(VERBOSE)cd $(CONTRIB_DIR); git rebase origin/$(GIT_BRANCH)

$(SVN_REV)/%:
	$(VERBOSE)svn co -r $(SVN_REV) $(SVN_URI)/$* $@

checkout_base:
ifeq ($(SVN_REV),)
	$(error Syntax 'make checkout SVN_REV=<revision>')
endif
	$(VERBOSE)svn co -r $(SVN_REV) --depth immediates $(SVN_URI) $(SVN_REV)
	$(VERBOSE)svn co -r $(SVN_REV) --depth files $(SVN_URI)/l4 $(SVN_REV)/l4
	$(VERBOSE)svn co -r $(SVN_REV) --depth files $(SVN_URI)/l4/pkg $(SVN_REV)/l4/pkg

checkout: checkout_base $(addprefix $(SVN_REV)/,$(SVN_TARGETS))
	$(VERBOSE)$(GNU_FIND) $(SVN_REV) -depth -name .svn -type d -exec rm -r {} ";"

help::
	$(ECHO)
	$(ECHO) "Check out upstream source code of Fiasco.OC"
	$(ECHO)
	$(ECHO) "The source code will be located at the '$(CONTRIB_DIR)/' directory."
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare                 - checkout upstream source codes"
	$(ECHO) "checkout SVN_REV=<rev>  - checkout out <rev> revision of Fiasco.OC/L4RE from the official repositiry"
	$(ECHO) "                          and store in directory <rev>"
	$(ECHO) "clean                   - remove upstream source codes"
	$(ECHO)

clean::
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

.NOTPARALLEL:
