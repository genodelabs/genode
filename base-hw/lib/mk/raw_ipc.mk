#
# \brief  Inter-process communication without thread implementations
# \author Martin Stein
# \date   2012-04-16
#

# add C++ source files
SRC_CC += ipc.cc ipc_marshal_cap.cc

# declare source paths
vpath ipc.cc              $(REP_DIR)/src/base
vpath ipc_marshal_cap.cc $(BASE_DIR)/src/base/ipc

