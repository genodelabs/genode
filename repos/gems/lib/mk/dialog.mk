SRC_CC += sandboxed_runtime.cc text_area_widget.cc
LIBS   += sandbox

vpath %.cc $(REP_DIR)/src/lib/dialog

SHARED_LIB = yes
