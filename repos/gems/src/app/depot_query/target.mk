TARGET  := depot_query
SRC_CC  := main.cc query_image_index.cc
LIBS    += base vfs
INC_DIR += $(REP_DIR)/src/app/fs_query $(PRG_DIR)
