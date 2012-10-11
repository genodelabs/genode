VANCOUVER_REV     = a7f4c2de42247e7a7c6ddb27a48f8a7d93d469ba
VANCOUVER         = vancouver-git
VANCOUVER_URL     = https://github.com/TUD-OS/NUL.git

#
# Check for tools
#
$(call check_tool,git)

#
# Interface to top-level prepare Makefile
#
PORTS += $(VANCOUVER)

#
# We need to execute some python scripts for preparing the i82576vf
# device model.
#
PYTHON2 := $(notdir $(lastword $(shell which python2 python2.{4,5,6,7,8})))
ifeq ($(PYTHON2),)
prepare: python_not_installed
python_not_installed:
	$(ECHO) "Error: Vancouver needs Python 2 to be installed"
	@false;
endif

prepare:: $(CONTRIB_DIR)/$(VANCOUVER)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(VANCOUVER)/.git:
	$(VERBOSE)git clone $(VANCOUVER_URL) $(DOWNLOAD_DIR)/$(VANCOUVER) && \
	cd download/vancouver-git && \
	git reset --hard $(VANCOUVER_REV) && \
	cd ../.. && touch $@

I82576VF_DIR = $(CONTRIB_DIR)/$(VANCOUVER)/julian/model/82576vf

$(CONTRIB_DIR)/$(VANCOUVER)/.git: $(DOWNLOAD_DIR)/$(VANCOUVER)/.git
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(VANCOUVER) $(CONTRIB_DIR)/$(VANCOUVER)
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(VANCOUVER) -N -p1 < src/vancouver/rename-82576-i82576.patch
	@# fix python version in code generator scripts
	$(VERBOSE)sed -i "s/env python2/env $(PYTHON2)/" $(I82576VF_DIR)/genreg.py
	$(VERBOSE)sed -i "s/env python2/env $(PYTHON2)/" $(I82576VF_DIR)/genreg2.py
	@# call code generators
	$(VERBOSE)cd $(I82576VF_DIR); \
	          ./genreg.py reg_pci.py ../../../vancouver/include/model/82576vfpci.inc
	$(VERBOSE)cd $(I82576VF_DIR); \
	          ./genreg.py reg_mmio.py ../../../vancouver/include/model/82576vfmmio.inc
	$(VERBOSE)cd $(I82576VF_DIR); \
	          ./genreg2.py reg_mmio.py ../../../vancouver/include/model/82576vfmmio_vnet.inc

$(CONTRIB_DIR)/$(VANCOUVER): $(CONTRIB_DIR)/$(VANCOUVER)/.git
