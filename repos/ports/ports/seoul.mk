SEOUL_BRANCH  = genode_13_08
SEOUL         = seoul-git
SEOUL_URL     = https://github.com/alex-ab/seoul.git

#
# Check for tools
#
$(call check_tool,git)

#
# Interface to top-level prepare Makefile
#
PORTS += $(SEOUL)

#
# We need to execute some python scripts for preparing the i82576vf
# device model.
#
PYTHON2 := $(notdir $(lastword $(shell which python2 python2.{4,5,6,7,8})))
ifeq ($(PYTHON2),)
prepare: python_not_installed
python_not_installed:
	$(ECHO) "Error: Seoul needs Python 2 to be installed"
	@false;
endif

prepare-seoul: fetch-new-version $(CONTRIB_DIR)/$(SEOUL)/genode_prepared

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SEOUL):
	$(VERBOSE)git clone $(SEOUL_URL) $(DOWNLOAD_DIR)/$(SEOUL) && \
	cd $(DOWNLOAD_DIR)/$(SEOUL) && \
	git checkout $(SEOUL_BRANCH) && \
	rm -f $(CURDIR)/$(CONTRIB_DIR)/$(SEOUL)/genode_prepared

fetch-new-version: $(DOWNLOAD_DIR)/$(SEOUL)
	$(VERBOSE)(cd download/seoul-git && \
	git fetch origin --dry-run 2>&1 | grep "$(SEOUL_BRANCH)" && \
	git pull origin && \
	rm -f $(CURDIR)/$(CONTRIB_DIR)/$(SEOUL)/genode_prepared) | true

I82576VF_DIR = $(CONTRIB_DIR)/$(SEOUL)/model/intel82576vf
EXECUTOR_DIR = $(CONTRIB_DIR)/$(SEOUL)/executor

$(CONTRIB_DIR)/$(SEOUL):
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(SEOUL) $(CONTRIB_DIR)/$(SEOUL) && \
	cd $(CONTRIB_DIR)/$(SEOUL) && \
	git checkout $(SEOUL_BRANCH)

$(CONTRIB_DIR)/$(SEOUL)/genode_prepared: $(CONTRIB_DIR)/$(SEOUL)
	$(VERBOSE)cd $(CONTRIB_DIR)/$(SEOUL) && git checkout -f $(SEOUL_BRANCH)
	$(VERBOSE)echo "fix python version in code generator scripts ..." && \
	sed -i "s/env python2/env $(PYTHON2)/" $(I82576VF_DIR)/genreg.py $(EXECUTOR_DIR)/build_instructions.py && \
	echo "call code generators ..." && \
	cd $(EXECUTOR_DIR) && \
	          ./build_instructions.py > instructions.inc && \
	cd $(CURDIR)/$(I82576VF_DIR) && \
	          ./genreg.py reg_pci.py ../../include/model/intel82576vfpci.inc && \
	cd $(CURDIR)/$(I82576VF_DIR) && \
	          ./genreg.py reg_mmio.py ../../include/model/intel82576vfmmio.inc && \
	touch $(CURDIR)/$@

.PHONY: fetch-new-version

clean-seoul:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SEOUL)
