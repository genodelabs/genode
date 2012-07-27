#
# \brief  Checkout and patch L4Linux/L4android source code
# \author Stefan Kalkowski
# \date   2011-03-10

CONTRIB_DIR     = contrib
VERBOSE        ?= @
ECHO            = @echo
TARGET         ?= l4linux

PATCH-l4linux   = $(addprefix patches/,l4lx_genode.patch icmp_align.patch tcp_mem.patch)
REV-l4linux     = 23
REPO-l4linux    = http://svn.tudos.org/repos/oc/l4linux/trunk

PATCH-l4android = patches/l4android_genode.patch
REV-l4android   = 90ca43bd629452ffd38d85fe8c976f069b5bb3d9
REPO-l4android  = git://git.l4android.org/kernel.git

UPDATE_PATCH    = $(word 1, $(PATCH-$(TARGET)))

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,patch)

ifeq ($(TARGET), l4android)
$(call check_tool,git)
DIFF   = git diff
UPDATE = cd $(REAL_CONTRIB_DIR)/l4android; git fetch; git reset --hard $(REV-l4android)
PATCH  = patch -p1
else
$(call check_tool,svn)
FILTER_DIR = net
DIFF_PATH  = $(filter-out $(FILTER_DIR),$(shell ls $(REAL_CONTRIB_DIR)/l4linux))
DIFF       = svn diff $(DIFF_PATH)
UPDATE     = svn up -r $(REV-l4linux)
PATCH      = patch -p0
endif


# realpath is there to follow symlink; if contrib dir does not exists yet,
# create new directory
REAL_CONTRIB_DIR := $(realpath $(CONTRIB_DIR))
ifeq ($(REAL_CONTRIB_DIR),)
REAL_CONTRIB_DIR := $(CONTRIB_DIR)
endif

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the L4Linux/L4Android repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare              - checkout and patch the L4Linux/L4Android source code"
	$(ECHO) "update-patch         - updates patch to the original code"
	$(ECHO) "clean                - revert patch from fetched code"
	$(ECHO) "cleanall             - delete all fetched code"
	$(ECHO)
	$(ECHO) "Set the variable TARGET=l4android if you want to checkout,"
	$(ECHO) "update, or clean L4Android in contrast to L4Linux code."
	$(ECHO)

prepare: clean
	$(VERBOSE)$(UPDATE)
	$(ECHO) "applying patches to '$(REAL_CONTRIB_DIR)/$(TARGET)'"
	$(VERBOSE)for i in $(PATCH-$(TARGET)); do $(PATCH) -d $(REAL_CONTRIB_DIR)/$(TARGET) < $$i; done
	$(VERBOSE)ln -s $(shell pwd)/src/drivers $(REAL_CONTRIB_DIR)/$(TARGET)/arch/l4/drivers
	$(ECHO)
	$(ECHO) "Preparation completed!"
	$(ECHO) "Now, go to your Genode build directory and type 'make $(TARGET)'."
	$(ECHO) "Hint: don't forget to put '$(shell pwd)' "
	$(ECHO) "      as a repository into your build.conf"
	$(ECHO)

$(CONTRIB_DIR):
	$(VERBOSE)mkdir -p $@

$(REAL_CONTRIB_DIR)/l4linux:
	$(VERBOSE)svn co $(REPO-l4linux) -r $(REV-l4linux) $@

$(REAL_CONTRIB_DIR)/l4android:
	$(VERBOSE)git clone $(REPO-l4android) $@

update-patch:
	$(ECHO) "Save changes to original code in $(UPDATE_PATCH)"
	$(VERBOSE)(cd $(REAL_CONTRIB_DIR)/$(TARGET); LC_COLLATE=C $(DIFF)) > $(UPDATE_PATCH) || true

clean: clean-$(TARGET)
	$(VERBOSE)rm -f $(CONTRIB_DIR)/$(TARGET)/arch/l4/drivers

clean-l4linux: $(REAL_CONTRIB_DIR)/$(TARGET)
	$(VERBOSE)svn revert -R $(REAL_CONTRIB_DIR)/l4linux
	$(VERBOSE)svn stat $(REAL_CONTRIB_DIR)/l4linux | sed -e 's/? //' | xargs rm -f

clean-l4android: $(REAL_CONTRIB_DIR)/$(TARGET)
	$(VERBOSE)cd $(REAL_CONTRIB_DIR)/l4android; git checkout -f

cleanall:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)
