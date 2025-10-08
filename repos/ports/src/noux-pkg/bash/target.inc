CONFIGURE_ARGS = --without-bash-malloc

# Prevent call of missing 'uselocale()'
CONFIGURE_ARGS += --disable-nls

#
# Avoid aliasing with the libc 'getenv' to prevent the dynamic linker from
# resolving libc-internal references to the bash binary, which causes
# trouble for fork/execve.
#
CONFIGURE_ARGS += bash_cv_getenv_redef=no

# Prevent deadlock when entering a command
CONFIGURE_ARGS += bash_cv_getcwd_malloc=yes

# Prevent call of missing 'getpgrp()'
CONFIGURE_ARGS += bash_cv_job_control_missing=missing

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

# Pass the correct strip program to 'install-sh'
MAKE_ENV += "STRIPPROG=$(STRIP)"

include $(call select_from_repositories,mk/noux.mk)
