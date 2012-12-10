#
# \brief  Essential platform specific sources for common programs
# \author Martin Stein
# \date   2012-04-16
#

# add libraries
LIBS += cxx lock

# add C++ sources
SRC_CC += _main.cc

# add assembly sources
SRC_S += crt0.s syscall.cc

# add include paths
INC_DIR += $(REP_DIR)/src/platform $(BASE_DIR)/src/platform

# declare source paths
vpath crt0.s     $(REP_DIR)/src/platform
vpath _main.cc   $(BASE_DIR)/src/platform
vpath syscall.cc $(REP_DIR)/src/base/arm
