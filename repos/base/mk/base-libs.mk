#
# Genode base libaries
#
# These static libraries are filtered out when linking dynamically linked
# binaries.
#
BASE_LIBS += cxx timed_semaphore alarm

#
# Name of Genode's dynamic linker
#
DYNAMIC_LINKER = ld
