LIBC_NET_DIR = $(LIBC_DIR)/libc/net

# needed for compiling getservbyname() and getservbyport()
SRC_C = getservent.c nsdispatch.c nsparser.c nslexer.c

# needed for getaddrinfo()
#SRC_C += getaddrinfo.c

# getaddrinfo() includes parts of rpc/ which in return includes
# even more stuff from the rpc framework. For now the effort for
# getting this right is too much.

# needed for gethostbyname()
SRC_C += gethostnamadr.c gethostbydns.c gethostbyht.c map_v4v6.c

# needed for getprotobyname()
SRC_C += getprotoent.c getprotoname.c

# defines in6addr_any
SRC_C += vars.c

# b64_ntop
SRC_C += base64.c

INC_DIR += $(REP_DIR)/include/libc/sys

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_NET_DIR)
