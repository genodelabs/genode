SRC_C = lock.cc semaphore.cc panic.cc printf.cc interrupt.cc pgtab.cc \
        memory.cc thread.cc pci_tree.cc pci.cc resources.cc timer.cc \
        dde_kit.cc spin_lock.cc
LIBS  = base alarm

#
# Enable 'spin_lock.cc' to reuse the spinlock implementation of the base
# repository by including the corresponding non-public headers local to
# 'base-<platform>/src'. Because the platform-specific parts of these headers
# may contain code that invokes system calls, we need to specify the 'syscall'
# lib as well. This way, the needed include-search directories are supplied to
# the build system via the respective 'import-syscall.mk' file.
#
LIBS        += syscall
REP_INC_DIR += src/base/lock src/platform

vpath % $(REP_DIR)/src/lib/dde_kit
