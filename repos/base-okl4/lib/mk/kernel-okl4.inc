LIBS             = kernel-okl4-include
SRC_O           += $(addprefix asm/,$(addsuffix .o,$(basename $(SRC_SPP))))
SRC_CC          += $(subst $(OKL4_SRC_DIR)/,,$(wildcard $(OKL4_SRC_DIR)/pistachio/kdb/src/*.cc)) \
                   $(subst $(OKL4_SRC_DIR)/,,$(wildcard $(OKL4_SRC_DIR)/pistachio/src/*.cc))
CONFIG          += __API__=v4 \
                   CONFIG_ASSERT_LEVEL=3 \
                   CONFIG_DEBUG=1 \
                   CONFIG_ENABLE_FASTPATHS=1 \
                   CONFIG_HYBRID_MUTEXES=1 \
                   CONFIG_IS_32BIT=1 \
                   CONFIG_KDB=1 \
                   CONFIG_KDB_BREAKIN=1 \
                   CONFIG_KDB_CLI=1 \
                   CONFIG_KDB_COLOR_VT=1 \
                   CONFIG_KDB_CONS=1 \
                   CONFIG_KMEM_TRACE=1 \
                   CONFIG_LITTLEENDIAN=1 \
                   CONFIG_MAX_SPACES=256U \
                   CONFIG_MAX_THREAD_BITS=10 \
                   CONFIG_MUTEX_NAMES=1 \
                   CONFIG_REMOTE_MEMORY_COPY=1 \
                   CONFIG_SCHEDULE_INHERITANCE=1 \
                   CONFIG_SMP_MAX_CPUS=1 \
                   CONFIG_THREAD_NAMES=1 \
                   CONFIG_TRACEBUFFER=1 \
                   CONFIG_TRACEBUF_PAGES=64 \
                   CONFIG_TRACEPOINTS=1 \
                   KENGE_PISTACHIO \
                   KERNEL_GEN_DAY=$(shell date +%d) \
                   KERNEL_GEN_MONTH=$(shell date +%m) \
                   KERNEL_GEN_YEAR=$(shell date +%g) \
                   KERNEL_SUBSUBVERSION=0 \
                   KERNEL_SUBVERSION=1 \
                   KERNEL_VERSION=0 \
                   WORDSIZE_32
CC_OPT          += -Wno-write-strings -Wredundant-decls -Wundef \
                   -Wpointer-arith -Wno-uninitialized \
                   -fno-builtin -fomit-frame-pointer \
                   -fno-exceptions -fno-unwind-tables \
                   -fno-asynchronous-unwind-tables \
                   -finline-limit=99999999 $(addprefix -D,$(CONFIG)) \
                   "-D__USER__=\"Genode Labs\""
CC_OPT          += -DCONFIG_MAX_THREAD_BITS=10

CC_OPT_PIC       =

CC_WARN := -Wall -Wno-unused-but-set-variable -Wno-uninitialized

#
# Enforce building the kernel with -O3. Otherwise, the kernel build would fail
# if the global 'CC_OLEVEL' is set is -O0. (OKL4 depends on some builtin
# functions that are not provided by the compiler when building with -O0)
#
override CC_OLEVEL = -O3

asm/%.o: $(OKL4_SRC_DIR)/%.spp
	$(MSG_COMP)$*.spp
	$(VERBOSE)$(CC) $(CC_MARCH) -x assembler-with-cpp -DASSEMBLY \
                    $(addprefix -D,$(CONFIG)) \
                    $(addprefix -I,$(INC_DIR)) -c $< -o $@

vpath %.cc $(OKL4_SRC_DIR)
