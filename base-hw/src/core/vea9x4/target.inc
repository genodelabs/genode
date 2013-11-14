#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/vea9x4
INC_DIR += $(REP_DIR)/src/core/arm

# add C++ sources
SRC_CC += platform_services.cc \
          platform_support.cc \
          cpu_support.cc

# add assembly sources
SRC_S += crt0.s \
         boot_modules.s \
         mode_transition.s

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
vpath mode_transition.s $(REP_DIR)/src/core/arm_v7
vpath crt0.s            $(REP_DIR)/src/core/arm
vpath cpu_support.cc    $(REP_DIR)/src/core/arm

# include less specific target parts
include $(REP_DIR)/src/core/target.inc

