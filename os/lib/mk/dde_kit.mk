SRC_C    = lock.cc semaphore.cc panic.cc printf.cc interrupt.cc pgtab.cc \
           memory.cc thread.cc pci_tree.cc pci.cc resources.cc timer.cc \
           dde_kit.cc spin_lock.cc
LIBS     = thread alarm

vpath % $(REP_DIR)/src/lib/dde_kit
