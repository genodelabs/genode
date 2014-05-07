HOST_INC_DIR += $(dir $(call select_from_repositories,src/platform/linux_syscalls.h))
HOST_INC_DIR += /usr/include

# needed for Ubuntu >= 11.04
HOST_INC_DIR += /usr/include/$(shell gcc -dumpmachine)
HOST_INC_DIR += /usr/include/i386-linux-gnu

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

