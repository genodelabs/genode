CONFIGURE_ARGS = --without-bash-malloc
CFLAGS += -Dsh_xfree=free

#
# Do not expect the environment to be passed as third argument
# to the main function. The Genode startup code does only
# deliver 'argc' and 'argv'.
#
CFLAGS += -DNO_MAIN_ENV_ARG=1

# Prevent interactions with nonexisting tty driver
CFLAGS += -DNO_TTY_DRIVER

include $(REP_DIR)/mk/noux.mk
