TARGET   ?= core
CORE_LIB ?= core.a

BUILD_ARTIFACTS := $(CORE_LIB)

$(TARGET):
	@true

ifneq ($(INSTALL_DIR),)
ifneq ($(DEBUG_DIR),)
$(TARGET): $(INSTALL_DIR)/$(CORE_LIB) $(DEBUG_DIR)/$(CORE_LIB)

$(CORE_LIB).stripped: $(CORE_LIB)
	$(VERBOSE)$(STRIP) --strip-unneeded -o $@ $<

$(INSTALL_DIR)/$(CORE_LIB) : $(CORE_LIB).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $(INSTALL_DIR)/$(CORE_LIB)

$(DEBUG_DIR)/$(CORE_LIB) : $(CORE_LIB)
	$(VERBOSE)ln -sf $(CURDIR)/$< $(DEBUG_DIR)/$(CORE_LIB)
endif
endif

#
# Create an archive from base and core archive objects, this archive is
# later linked to core. This gives better performance than the old way where we
# would link the archives into a relocatable object file (ld -r) that in turn
# was linked to core - leading to a large object file with many relocations
# causing long link times (especially on base-hw)
#
# We create the archive by piping an MRI script to ar -M
# "create " - create library
# "addlib"  - add $(LINK_ITEMS)
# "save"    - save and overwrite library
.PHONY: $(CORE_LIB)
$(CORE_LIB):
	$(VERBOSE)(echo    "create $@"; \
	           echo -e "$(addprefix \naddlib ,$(LINK_ITEMS))"; \
	           echo    "save"; \
	           echo    "end"; \
	          ) | $(AR) -M


clean cleanall:
	$(VERBOSE)rm -f $(CORE_LIB) $(CORE_LIB).stripped
