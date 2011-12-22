SRC_CC     = cap_sel_alloc.cc main_thread.cc context_area.cc echo.cc \
             thread_nova.cc thread.cc
LIBS      += thread_context
INC_DIR   += $(REP_DIR)/src/core/include

vpath cap_sel_alloc.cc  $(REP_DIR)/src/base/env
vpath main_thread.cc    $(REP_DIR)/src/base/env
vpath thread.cc        $(BASE_DIR)/src/base/thread
vpath thread_nova.cc    $(REP_DIR)/src/test
vpath context_area.cc   $(REP_DIR)/src/test
vpath echo.cc           $(REP_DIR)/src/core
