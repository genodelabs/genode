
GMP_MPN_DIR = $(GMP_DIR)/mpn

# this file uses the 'sdiv_qrnnd' symbol which is not defined
FILTER_OUT += udiv_w_sdiv.c

# add ARM-specific assembly files and filter out the generic C files if needed

SRC_ASM += copyd.asm copyi.asm invert_limb.asm

FILTER_OUT += popham.c
FILTER_OUT += pre_divrem_1.c logops_n.c sec_div.c sec_pi1_div.c copyi.c copyd.c

SRC_C += $(notdir $(wildcard $(REP_DIR)/src/lib/gmp/mpn/spec/64bit/*.c))
SRC_C += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(GMP_MPN_DIR)/generic/*.c)))

include $(REP_DIR)/lib/mk/gmp.inc

PWD := $(shell pwd)

SRC_O += $(SRC_ASM:.asm=.o) hamdist.o popcount.o
#
# Create execution environment for the m4-ccas tool, which is used by the gmp
# library to assemble asm files to object files. Make sure to execute this rule
# only from the actual build pass (not when called from 'dep_lib.mk'). This
# way, the 'm4env' gets created within the local build directory of the
# library, not the global build directory.
#
ifeq ($(called_from_lib_mk),yes)
all: m4env
endif

m4env:
	$(VERBOSE)mkdir -p $@/mpn/arm_64
	$(VERBOSE)ln -s $(REP_DIR)/src/lib/gmp/spec/arm_64/config.m4 m4env
	$(VERBOSE)ln -s $(GMP_MPN_DIR)/asm-defs.m4 m4env/mpn

#
# Create object files out of asm files
#
ifneq ($(VERBOSE),)
M4_OUTPUT_FILTER = > /dev/null
endif

hamdist.o popcount.o: popham.c
	$(MSG_COMP)$@
	$(VERBOSE)$(CC) $(CC_DEF) $(CC_C_OPT) -DOPERATION_${@:.o=} $(INCLUDES) -c $< -o $@

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)cd m4env/mpn; \
		$(GMP_MPN_DIR)/m4-ccas --m4=m4 $(CC) $(CC_MARCH) -std=gnu99 -fPIC -DPIC $(CC_OPT_$*) $(INCLUDES) -c $< -o $(PWD)/$@ \
			$(M4_OUTPUT_FILTER)

vpath %.c   $(REP_DIR)/src/lib/gmp/mpn/spec/64bit
vpath %.c   $(GMP_MPN_DIR)/generic
vpath %.asm $(GMP_MPN_DIR)/arm64

CC_CXX_WARN_STRICT =
