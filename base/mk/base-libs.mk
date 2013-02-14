#
# Genode base libaries
#
# These linked against 'ldso' and filtered out for dynamically
# linked binaries
#
BASE_LIBS = base-common base cxx timed_semaphore alarm

#
# Name of Genode's dynamic linker
#
DYNAMIC_LINKER = ld
