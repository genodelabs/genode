LUA     = lua-5.1.5
LUA_TGZ = $(LUA).tar.gz
LUA_URL = http://www.lua.org/ftp/$(LUA_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LUA)

LUA_INC_DIR = include/lua

prepare-lua: $(CONTRIB_DIR)/$(LUA) $(LUA_INC_DIR)

$(CONTRIB_DIR)/$(LUA): clean-lua

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LUA_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LUA_URL) && touch $@

$(CONTRIB_DIR)/$(LUA): $(DOWNLOAD_DIR)/$(LUA_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

LUA_INCLUDES = lua.h lauxlib.h luaconf.h lualib.h

$(LUA_INC_DIR):
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in $(LUA_INCLUDES); do \
		ln -sf ../../$(CONTRIB_DIR)/$(LUA)/src/$$i $@; done

clean-lua:
	$(VERBOSE)rm -rf $(LUA_INC_DIR)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LUA)
