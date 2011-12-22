SRC_CC   = process.cc
LIBS     = syscall

#
# The Linux version of the process library does not use Genode's ELF loader for
# loading executables but the 'execve' system call. However, for supporting
# dynamically linked executables, we have to take the decision of whether to load
# the dynamic linker or a static executable based on information provided by
# the ELF program header. We use the ELF library to obtain this information.
#
LIBS    += elf

vpath process.cc $(REP_DIR)/src/base/process
