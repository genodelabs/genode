LIBC_GEN_DIR = $(LIBC_DIR)/lib/libc/gen

# this file produces a warning about a missing header file, lets drop it
FILTER_OUT_C += getosreldate.c sem.c valloc.c getpwent.c

# the following are superceded by our minimalist sysctl and sysconf
FILTER_OUT_C += \
	devname.c feature_present.c getpagesizes.c getvfsbyname.c \
	setproctitle.c sysconf.c

# the following is a wrapper over rusage, which we cannot provide
FILTER_OUT_C += clock.c

# we implement this ourselves
FILTER_OUT_C += isatty.c

# compatibility with older FreeBSD is not a concern
FILTER_OUT_C += $(notdir $(wildcard $(LIBC_GEN_DIR)/*-compat11.c))

# file-locking not supported
FILTER_OUT_C += lockf.c

# interposing stubs
FILTER_OUT_C += \
	elf_utils.c \
	sleep.c \
	_spinlock_stub.c \
	usleep.c \
	wait3.c \
	wait.c \
	waitid.c \
	waitpid.c \

# not appropriate in this context
FILTER_OUT_C += \
	cap_sandboxed.c \
	daemon.c \
	disklabel.c \
	exect.c \
	getentropy.c \
	getutxent.c \
	pututxline.c \
	readpassphrase.c \
	scandir_b.c \
	sem_new.c \
	signal.c \
	siginterrupt.c \
	trivial-getcontextx.c \
	utxdb.c \

# Uses non-standard "thr_kill"
FILTER_OUT_C += raise.c

SRC_C = $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_GEN_DIR)/*.c)))

SRC_C += interposing_table.c

# 'sysconf.c' includes the local 'stdtime/tzfile.h'
INC_DIR += $(REP_DIR)/src/lib/libc/stdtime

# '_pthread_stubs.c' includes the local 'libc_pdbg.h'
INC_DIR += $(REP_DIR)/src/lib/libc

INC_DIR += $(LIBC_DIR)/sys/crypto/chacha20
INC_DIR += $(LIBC_DIR)/sys

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_GEN_DIR)
vpath interposing_table.c $(LIBC_DIR)/lib/libc/sys
