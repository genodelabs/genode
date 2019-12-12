HOST_INC_DIR += $(dir $(call select_from_repositories,src/lib/syscall/linux_syscalls.h))
HOST_INC_DIR += $(dir $(CUSTOM_HOST_CC))/../$(shell $(CUSTOM_HOST_CC) -dumpmachine)/libc/usr/include
HOST_INC_DIR += /usr/include

# needed for Ubuntu >= 11.04
HOST_INC_DIR += /usr/include/$(shell $(CUSTOM_HOST_CC) -dumpmachine)

#
# Explicitly add some well-known paths as the dumpmachine magic above does not
# suffice on all Linux distros (e.g., Debian Stretch).
#
HOST_INC_DIR += /usr/include/i386-linux-gnu
HOST_INC_DIR += /usr/include/x86_64-linux-gnu

#
# Some header files installed on GNU/Linux test for the GNU compiler. For
# example, 'stdio.h' might complain with the following error otherwise:
#
#  /usr/include/stdio.h:432:27: error: expected initializer before ‘throw’
#  /usr/include/stdio.h:488:6: error: expected initializer before ‘throw’
#
# By manually defining '_GNU_SOURCE', the header files are processed as
# expected.
#
CC_OPT += -D_GNU_SOURCE

