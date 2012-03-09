#
# Genode base libaries
#
# These linked against 'ldso' and filtered out for dynamically
# linked binaries
#
BASE_LIBS = alarm allocator_avl avl_tree cxx env heap \
            ipc lock slab timed_semaphore thread signal \
            log_console slab cap_copy

#
# Name of Genode's dynamic linker
#
DYNAMIC_LINKER = ld
