#
# Before including this file, the CONTENT variable may contain a list of
# repository-relative paths of content to include in the src archive in
# addition to the default content inferred for the given board.
#


#
# Determine board name from the path to the src recipe's content.mk file
#

# obtain directory name of src recipe w/o trailing slash
_BOARD := $(CONTENT_MK:/content.mk=)

# strip all leading path elements, keeping only the src-recipe name
_BOARD := $(notdir $(_BOARD))

# remove "base-hw-" part of the src-recipe name, keeping only the board name
BOARD := $(patsubst base-hw-%,%,$(_BOARD))


#
# Selection of ARCH-specific subdirectories and files
#

LIB_MK_SPECS(arm_v6)  := arm arm_v6
LIB_MK_SPECS(arm_v7a) := arm arm_v7 cortex_a8 cortex_a9 cortex_a15
LIB_MK_SPECS(arm_v8a) := arm_64 arm_v8
LIB_MK_SPECS(x86_64)  := x86_64
LIB_MK_SPECS(riscv)   := riscv

SRC_BOOTSTRAP_SPECS(arm_v6)  := arm
SRC_BOOTSTRAP_SPECS(arm_v7a) := arm
SRC_BOOTSTRAP_SPECS(arm_v8a) := arm_64 arm
SRC_BOOTSTRAP_SPECS(x86_64)  := x86_64
SRC_BOOTSTRAP_SPECS(riscv)   := riscv

SRC_CORE_SPECS(arm_v6)  := arm arm_v6
SRC_CORE_SPECS(arm_v7a) := arm arm_v7 cortex_a8 cortex_a9 cortex_a15
SRC_CORE_SPECS(arm_v8a) := arm arm_v8
SRC_CORE_SPECS(x86_64)  := x86 x86_64
SRC_CORE_SPECS(riscv)   := riscv

SRC_INCLUDE_HW_SPECS(arm_v6)  := arm
SRC_INCLUDE_HW_SPECS(arm_v7a) := arm
SRC_INCLUDE_HW_SPECS(arm_v8a) := arm arm_64
SRC_INCLUDE_HW_SPECS(x86_64)  := x86_64
SRC_INCLUDE_HW_SPECS(riscv)   := riscv

SRC_LIB_BASE_SPECS(arm_v6)  := arm
SRC_LIB_BASE_SPECS(arm_v7a) := arm
SRC_LIB_BASE_SPECS(arm_v8a) := arm_64
SRC_LIB_BASE_SPECS(x86_64)  := x86_64
SRC_LIB_BASE_SPECS(riscv)   := riscv

SRC_LIB_STARTUP_SPECS(arm_v6)  := arm
SRC_LIB_STARTUP_SPECS(arm_v7a) := arm
SRC_LIB_STARTUP_SPECS(arm_v8a) := arm_64
SRC_LIB_STARTUP_SPECS(x86_64)  := x86_64
SRC_LIB_STARTUP_SPECS(riscv)   := riscv

# location of hw-ld.mk file
LD_MK_DIRS(arm_v6)  := lib/mk/spec/arm
LD_MK_DIRS(arm_v7a) := lib/mk/spec/arm
LD_MK_DIRS(arm_v8a) := lib/mk/spec/arm_v8
LD_MK_DIRS(x86_64)  := lib/mk/spec/x86_64
LD_MK_DIRS(riscv)   := lib/mk/spec/riscv


#
# Defermine CPU architecture for the given BOARD
#

BOARD_ARCH_PATH := $(REP_DIR)/board/$(BOARD)/arch

ifeq ($(wildcard $(BOARD_ARCH_PATH)),)
content: arch_undefined
arch_undefined:
	echo -n "\nError: CPU architecure for board $(BOARD) undefined\n\n" \
	        " missing file $(BOARD_ARCH_PATH)\n\n"
	false
else
SHELL := bash
ARCHS := $(sort $(shell cat $(BOARD_ARCH_PATH)))
endif


#
# Enumeration of package content
#

#
# Function to obtain the content for the board's supported architectures
#
# The parameter is the variable name for one of the subdirectories defined
# above. The 'sort' is merely used to remove duplicates.
#
selected_content = $(sort $(foreach A,$(ARCHS),${$1($A)}))

BASE_DIR    := $(GENODE_DIR)/repos/base
BASE_HW_DIR := $(GENODE_DIR)/repos/base-hw

SRC_TIMER := target.inc include hw main.cc

SRC_INCLUDE_HW += $(notdir $(wildcard $(BASE_HW_DIR)/src/include/hw/*.h)) \
                  $(addprefix spec/,${call selected_content,SRC_INCLUDE_HW_SPECS}) \
                  uart

SRC_BOOTSTRAP += hw env.cc init.cc lock.cc log.cc thread.cc \
                 platform.cc platform.h \
                 $(addprefix spec/,${call selected_content,SRC_BOOTSTRAP_SPECS}) \
                 $(addprefix board/,$(BOARD))

SRC_LIB_BASE += $(notdir $(wildcard $(BASE_HW_DIR)/src/lib/base/*.cc)) \
                $(notdir $(wildcard $(BASE_DIR)/src/lib/base/*.cc)) \
                ${call selected_content,SRC_LIB_BASE_SPECS}

SRC_LIB_TIMEOUT += duration.cc \
                   hw/timer_connection_timestamp.cc \
                   timeout.cc \
                   timer_connection.cc \
                   timer_connection_time.cc

SRC_LIB_STARTUP += _main.cc \
                   $(addprefix spec/,${call selected_content,SRC_LIB_STARTUP_SPECS})

SRC_CORE += $(notdir $(wildcard $(BASE_HW_DIR)/src/core/*.cc)) \
            $(notdir $(wildcard $(BASE_HW_DIR)/src/core/*.h)) \
            $(notdir $(wildcard $(BASE_DIR)/src/core/*.cc)) \
            $(addprefix spec/,${call selected_content,SRC_CORE_SPECS}) \
            $(addprefix board/,$(BOARD)) \
            version.inc target.inc include hw kernel

# names of the lib/mk/ files to consider for inclusion in the src archive
LIB_MK_FILES := base-common.inc base-hw-common.mk \
                base.inc base-hw.inc base-hw.mk \
                bootstrap-hw.inc bootstrap-hw-$(BOARD).inc bootstrap-hw-$(BOARD).mk \
                core-hw.inc core-hw-$(BOARD).inc core-hw-$(BOARD).mk \
                startup.inc startup-hw.mk \
                timeout-hw.mk cxx.mk ld-hw.mk syscall-hw.mk

LIB_MK_DIRS := lib/mk $(addprefix lib/mk/spec/,${call selected_content,LIB_MK_SPECS})

CONTENT += $(foreach D,$(LIB_MK_DIRS),$(addprefix $D/,$(LIB_MK_FILES)))

CONTENT += $(addprefix src/timer/,$(SRC_TIMER)) \
           $(addprefix src/include/hw/,$(SRC_INCLUDE_HW)) \
           $(addprefix src/bootstrap/,$(SRC_BOOTSTRAP)) \
           $(addprefix lib/mk/,$(LIB_MK)) \
           $(addprefix src/lib/base/,$(SRC_LIB_BASE)) \
           $(addprefix src/lib/timeout/,$(SRC_LIB_TIMEOUT)) \
           $(addprefix src/lib/startup/,$(SRC_LIB_STARTUP)) \
           $(addprefix src/core/,$(SRC_CORE)) \
           src/lib/hw src/lib/ld src/lib/cxx \
           src/include/base/internal src/include/pager \
           src/include/hw_native_vcpu \
           include/drivers/uart

# remove duplicates
CONTENT := $(sort $(CONTENT))


#
# Utilities to aggregate content from base, base-hw, and rep directories
#

_try_mirror_from = $(if $(wildcard $1/$2),mkdir -p $(dir $2); cp -r $1/$2 $(dir $2))

mirror_from_base_dir    = $(call _try_mirror_from,$(GENODE_DIR)/repos/base,$@)
mirror_from_base_hw_dir = $(call _try_mirror_from,$(GENODE_DIR)/repos/base-hw,$@)
mirror_from_rep_dir     = $(call _try_mirror_from,$(REP_DIR),$@)


#
# Rules
#

content: $(CONTENT)

$(CONTENT):
	$(mirror_from_base_dir)
	$(mirror_from_base_hw_dir)
	$(mirror_from_rep_dir)

content: LICENSE
LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

content: etc/specs.conf
etc/specs.conf:
	$(mirror_from_base_hw_dir)

content: generalize_target_names
generalize_target_names: $(CONTENT)
	# apply kernel-agnostic convention of naming the timer and ld.lib.so
	for subdir in ${call selected_content,LD_MK_DIRS}; do \
		mv $$subdir/ld-hw.mk $$subdir/ld.mk; done
	sed -i "s/hw_timer_drv/timer/" src/timer/hw/target.mk
	# supplement BOARD definition that normally comes form the build dir
	sed -i "s/\?= unknown/:= $(BOARD)/" src/core/hw/target.mk
	sed -i "s/\?= unknown/:= $(BOARD)/" src/bootstrap/hw/target.mk
	# discharge targets when building for mismatching architecture
	sed -i "1aREQUIRES := $(ARCH)"      src/core/hw/target.mk
	sed -i "1aREQUIRES     := $(ARCH)"  src/bootstrap/hw/target.mk
	sed -i "/REQUIRES/s/hw/hw $(ARCH)/" src/timer/hw/target.mk

