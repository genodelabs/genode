CONFIGURE_ARGS = --without-bash-malloc

#
# Avoid aliasing with the libc 'getenv' to prevent the dynamic linker from
# resolving libc-internal references to the bash binary, which causes
# trouble for fork/execve.
#
CONFIGURE_ARGS += bash_cv_getenv_redef=no

CFLAGS += -Dsh_xfree=free

#
# Do not expect the environment to be passed as third argument
# to the main function. The Genode startup code does only
# deliver 'argc' and 'argv'.
#
CFLAGS += -DNO_MAIN_ENV_ARG=1

# Prevent interactions with nonexisting tty driver
CFLAGS += -DNO_TTY_DRIVER

# Prevent link error with GCC 10, which defaults to -fno-common
CFLAGS += -fcommon

include $(call select_from_repositories,mk/noux.mk)
