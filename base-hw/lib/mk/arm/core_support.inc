#
# \brief  Parts of core that depend on ARM
# \author Martin Stein
# \date   2012-04-16
#

# add C++ sources
SRC_CC += syscall.cc

# add assembly sources
SRC_S += crt0.s boot_modules.s

#
# Check if there are other images wich shall be linked to core.
# If not use a dummy boot-modules file wich includes only the symbols.
#
ifeq ($(wildcard $(BUILD_BASE_DIR)/boot_modules.s),)
  vpath boot_modules.s $(REP_DIR)/src/core/arm
else
  INC_DIR += $(BUILD_BASE_DIR)
  vpath boot_modules.s $(BUILD_BASE_DIR)
endif

# declare source paths
vpath syscall.cc $(REP_DIR)/src/base/arm
vpath %          $(REP_DIR)/src/core/arm

