ifeq ($(CONTRIB_DIR),)
DRM_DIR = $(realpath $(call select_from_repositories,include/drm)/..)
INC_DIR += $(DRM_DIR) $(addprefix $(DRM_DIR)/,intel drm)
else
DRM_DIR = $(call select_from_ports,drm)/src/lib/drm
INC_DIR += $(DRM_DIR) $(addprefix $(DRM_DIR)/,intel include/drm)
endif
