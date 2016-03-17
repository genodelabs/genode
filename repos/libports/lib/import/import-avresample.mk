include $(call select_from_repositories,lib/import/import-av.inc)

REP_INC_DIR += include/libavresample
INC_DIR     += $(call select_from_ports,libav)/include/libav/libavresample
