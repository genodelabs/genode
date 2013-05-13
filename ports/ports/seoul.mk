SEOUL_REV     = 6ee3f454c66fa9fb427ff161ced3fccf6462311a
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

prepare:: $(CONTRIB_DIR)/$(SEOUL)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SEOUL)/.git:
	$(VERBOSE)git clone $(SEOUL_URL) $(DOWNLOAD_DIR)/$(SEOUL) && \
	cd download/seoul-git && \
	git reset --hard $(SEOUL_REV) && \
	cd ../.. && touch $@

I82576VF_DIR = $(CONTRIB_DIR)/$(SEOUL)/model/intel82576vf
EXECUTOR_DIR = $(CONTRIB_DIR)/$(SEOUL)/executor

$(CONTRIB_DIR)/$(SEOUL)/.git: $(DOWNLOAD_DIR)/$(SEOUL)/.git
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(SEOUL) $(CONTRIB_DIR)/$(SEOUL)
	@# fix python version in code generator scripts
	$(VERBOSE)sed -i "s/env python2/env $(PYTHON2)/" $(I82576VF_DIR)/genreg.py $(EXECUTOR_DIR)/build_instructions.py
	@# call code generators
	$(VERBOSE)cd $(EXECUTOR_DIR); \
                  ./build_instructions.py > instructions.inc
	$(VERBOSE)cd $(I82576VF_DIR); \
	          ./genreg.py reg_pci.py ../../include/model/intel82576vfpci.inc
	$(VERBOSE)cd $(I82576VF_DIR); \
	          ./genreg.py reg_mmio.py ../../include/model/intel82576vfmmio.inc

$(CONTRIB_DIR)/$(SEOUL): $(CONTRIB_DIR)/$(SEOUL)/.git
