LIBL4_DIR = $(CODEZERO_DIR)/conts/userlibs/libl4

SRC_C += init.c irq.c mutex.c
SRC_C += arch/arm/exregs.c
SRC_S += $(addprefix arch/arm/,syscalls.S new_thread.S)
SRC_C += $(addprefix lib/,addr.c bit.c idpool.c)
SRC_C += $(addprefix lib/thread/,init.c thread.c)
SRC_C += $(addprefix lib/cap/,cap.c read.c)

INC_DIR += $(CODEZERO_DIR)/conts/userlibs/libc/include
INC_DIR += $(CODEZERO_DIR)/conts/userlibs/libmem/include
INC_DIR += $(CODEZERO_DIR)/conts/userlibs/libmem

vpath % $(LIBL4_DIR)/src

#
# The libl4 source files uses macros defined in macros.h but do not
# explicitly include the 'macros.h' header file.
#
CC_OPT += -include $(LIBL4_DIR)/include/l4lib/macros.h

#
# Resolve conflicts with built-in functions
#
CC_OPT += -fno-builtin-pow

#
# During the compilation of the libl4 file 'thread.c', the 'l4id_t' type
# is used without prior inclusion of 'types.h'. Furthermore, 'types.h'
# has a wrong include guard, so we take care of this problem using a
# wrapper.
#
CC_OPT_lib_thread_thread += -include fix_include_types.h
CC_OPT_arch_arm_exregs   += -include fix_include_types.h

lib/thread/thread.o arch/arm/exregs.o: fix_include_types.h

fix_include_types.h:
	@echo "#include <l4lib/arch/arm/types.h>" > $@
	@echo "#define __L4LIB_ARM_TYPES_H___" >> $@

CC_OPT += -std=gnu99

