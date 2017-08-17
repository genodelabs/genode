DRM_DIR  =  $(call select_from_ports,drm)/src/lib/drm
INC_DIR += $(DRM_DIR) $(addprefix $(DRM_DIR)/,intel include/drm)
