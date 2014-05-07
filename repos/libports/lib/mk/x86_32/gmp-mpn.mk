
GMP_MPN_DIR = $(GMP_DIR)/mpn

# filter out to avoid compile errors
FILTER_OUT += pre_mod_1.c

# this file uses the 'sdiv_qrnnd' symbol which is not defined
FILTER_OUT += udiv_w_sdiv.c

# add Pentium-specific assembly files and filter out the generic C files if needed

SRC_ASM += copyd.asm copyi.asm hamdist.asm

SRC_ASM += popcount.asm
FILTER_OUT += popham.c

SRC_ASM += add_n.asm
FILTER_OUT += add_n.c
CC_OPT_add_n = -DOPERATION_add_n

SRC_ASM += sub_n.asm
FILTER_OUT += sub_n.c
CC_OPT_sub_n = -DOPERATION_sub_n

SRC_C += $(notdir $(wildcard $(REP_DIR)/src/lib/gmp/mpn/32bit/*.c))
SRC_C += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(GMP_MPN_DIR)/generic/*.c)))

include $(REP_DIR)/lib/mk/gmp.inc

PWD := $(shell pwd)

SRC_O += $(SRC_ASM:.asm=.o)

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
	$(VERBOSE)mkdir -p $@/mpn/x86
	$(VERBOSE)ln -s $(REP_DIR)/src/lib/gmp/x86_32/config.m4 m4env
	$(VERBOSE)ln -s $(GMP_MPN_DIR)/asm-defs.m4 m4env/mpn
	$(VERBOSE)ln -s $(GMP_MPN_DIR)/x86/x86-defs.m4 m4env/mpn/x86

#
# Create object files out of asm files
#
ifneq ($(VERBOSE),)
M4_OUTPUT_FILTER = > /dev/null
endif

%.o: %.asm
	$(MSG_ASSEM)$@
	$(VERBOSE)cd m4env/mpn; \
		$(GMP_MPN_DIR)/m4-ccas --m4=m4 $(CC) $(CC_MARCH) -std=gnu99 -fPIC -DPIC $(CC_OPT_$*) $(INCLUDES) -c $< -o $(PWD)/$@ \
			$(M4_OUTPUT_FILTER)

vpath %.c   $(REP_DIR)/src/lib/gmp/mpn/32bit
vpath %.c   $(GMP_MPN_DIR)/generic
vpath %.asm $(REP_DIR)/src/lib/gmp/mpn/x86_32
vpath %.asm $(GMP_MPN_DIR)/x86/pentium
vpath %.asm $(GMP_MPN_DIR)/x86
