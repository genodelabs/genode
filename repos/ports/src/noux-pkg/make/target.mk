#
# Prevent double definition of '__size_t' in 'glob/glob.h'
#
CPPFLAGS += -D__FreeBSD__

#
# Avoid aliasing with libc symbols to prevent the dynamic linker from
# resolving libc-internal references to the make binary, which causes
# trouble for fork/execve.
#
CPPFLAGS += $(foreach S, optarg optind opterr optopt, -D$S=mk_$S)

include $(call select_from_repositories,mk/noux.mk)
