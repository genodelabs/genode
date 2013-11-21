MESA_VERSION = 7.8.1
MESA         = MesaLib-$(MESA_VERSION)
MESA_DIR     = Mesa-$(MESA_VERSION)
MESA_TGZ     = $(MESA).tar.gz
MESA_URL     = ftp://ftp.freedesktop.org/pub/mesa/older-versions/7.x/7.8.1/$(MESA_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register Mesa port as lower case to be consistent with the
# other libraries.
#
PORTS += mesa-$(MESA_VERSION)

MESA_INCLUDE_SYMLINKS = $(addprefix include/,GL KHR EGL/egl.h EGL/eglext.h)

prepare-mesa: $(CONTRIB_DIR)/$(MESA_DIR) tool/mesa/glsl $(MESA_INCLUDE_SYMLINKS)

$(CONTRIB_DIR)/$(MESA_DIR): clean-mesa

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(MESA_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(MESA_URL) && touch $@

$(CONTRIB_DIR)/$(MESA_DIR): $(DOWNLOAD_DIR)/$(MESA_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR)
	$(VERBOSE)patch -N -p0 -d $(CONTRIB_DIR)/$(MESA_DIR) < src/lib/gallium/p_state_config.patch
	$(VERBOSE)patch -N -p1 -d $(CONTRIB_DIR)/$(MESA_DIR) < src/lib/egl/opengl_precision.patch
	$(VERBOSE)touch $@

tool/mesa/glsl:
	$(VERBOSE)make -C tool/mesa

$(MESA_INCLUDE_SYMLINKS):
	$(VERBOSE)ln -sf $(realpath $(CONTRIB_DIR)/$(MESA_DIR)/$@) $@ && touch $@

clean_tool_mesa:
	$(VERBOSE)make -C tool/mesa clean

clean_mesa_include_symlinks:
	$(VERBOSE)rm -f $(MESA_INCLUDE_SYMLINKS)

clean-mesa: clean_tool_mesa clean_mesa_include_symlinks
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(MESA_DIR)



