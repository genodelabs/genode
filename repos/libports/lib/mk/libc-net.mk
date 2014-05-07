LIBC_NET_DIR = $(LIBC_DIR)/libc/net

# needed for compiling getservbyname() and getservbyport()
SRC_C = getservent.c nsdispatch.c nsparser.c nslexer.c

# needed for getaddrinfo()
SRC_C += getaddrinfo.c

# needed for getnameinfo()
SRC_C += getnameinfo.c name6.c

# needed for gethostbyname()
SRC_C += gethostnamadr.c gethostbydns.c gethostbyht.c map_v4v6.c

# needed for getprotobyname()
SRC_C += getprotoent.c getprotoname.c

# defines in6addr_any
SRC_C += vars.c

# b64_ntop
SRC_C += base64.c

SRC_C += rcmd.c rcmdsh.c

include $(REP_DIR)/lib/mk/libc-common.inc

INC_DIR += $(REP_DIR)/include/libc
INC_DIR += $(REP_DIR)/include/libc/sys

# needed for name6.c, contains res_private.h
INC_DIR += $(LIBC_DIR)/libc/resolv

vpath %.c $(LIBC_NET_DIR)
