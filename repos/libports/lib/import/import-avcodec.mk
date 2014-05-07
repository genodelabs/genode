include $(call select_from_repositories,lib/import/import-av.inc)

INC_DIR += $(call select_from_ports,libav)/include/libav/libavcodec
