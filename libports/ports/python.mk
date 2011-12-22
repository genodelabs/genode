PYTHON     = python-2.6.4
PYTHON_TGZ = Python-2.6.4.tgz
PYTHON_URL = http://www.python.org/ftp/python/2.6.4/$(PYTHON_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(PYTHON)

prepare-python: $(CONTRIB_DIR)/$(PYTHON) include/python2.6

$(CONTRIB_DIR)/$(PYTHON): clean-python

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(PYTHON_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(PYTHON_URL) && touch $@

$(CONTRIB_DIR)/$(PYTHON): $(DOWNLOAD_DIR)/$(PYTHON_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR)
	@# rename Python subdirectory to lower case to be consistent
	@# with the other libs
	$(VERBOSE)mv $(CONTRIB_DIR)/Python-2.6.4 $@
	$(VERBOSE)touch $@
	$(VERBOSE)patch -p0 -i src/lib/python/posixmodule.patch

include/python2.6:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(PYTHON)/Include $@

clean-python:
	$(VERBOSE)rm -rf include/python2.6
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(PYTHON)
