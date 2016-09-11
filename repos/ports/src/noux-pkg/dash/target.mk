#
# Prevent double definition of BSD. This definition is specified at the
# compiler command line, but also in the 'sys/param.h' header of the FreeBSD
# libc.
#
CFLAGS = -UBSD

include $(REP_DIR)/mk/noux.mk
