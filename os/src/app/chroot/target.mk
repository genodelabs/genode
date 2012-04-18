TARGET   = chroot
REQUIRES = linux
SRC_CC   = main.cc
LIBS     = cxx env server lx_hybrid

#
# XXX find a way to remove superfluous warning:
#
# base/include/util/token.h: In constructor ‘Genode::Config::Config()’:
# base/include/util/token.h:69:67: warning: ‘ret’ may be used uninitialized in
#                                            this function [-Wuninitialized]
# base/include/base/capability.h:196:62: note: ‘ret’ was declared here
# base/include/util/token.h:100:68: warning: ‘prephitmp.1897’ may be used
#                                            uninitialized in this function
#                                            [-Wuninitialized]
# os/include/os/config.h:42:4: note: ‘prephitmp.1897’ was declared here
#
CC_WARN  = -Wall -Wno-uninitialized

