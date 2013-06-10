PYTHON          = python-2.6.4
PYTHON_TGZ      = Python-2.6.4.tgz
PYTHON_SIG      = $(PYTHON_TGZ).asc
PYTHON_BASE_URL = http://www.python.org/ftp/python/2.6.4
PYTHON_URL      = $(PYTHON_BASE_URL)/$(PYTHON_TGZ)
PYTHON_URL_SIG  = $(PYTHON_BASE_URL)/$(PYTHON_SIG)
PYTHON_KEY      = 12EF3DC38047DA382D18A5B999CDEA9DA4135B38

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

$(DOWNLOAD_DIR)/$(PYTHON_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(PYTHON_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(PYTHON_TGZ).verified: $(DOWNLOAD_DIR)/$(PYTHON_TGZ)
	#
	# As signatures are only provided for versions 2.7.3 and newer, the check
	# is yet disabled. Just remove the comment sign once the newer version is
	# used.
	#
	#$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(PYTHON_TGZ) $(DOWNLOAD_DIR)/$(PYTHON_SIG) $(PYTHON_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(PYTHON): $(DOWNLOAD_DIR)/$(PYTHON_TGZ)
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR)
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
