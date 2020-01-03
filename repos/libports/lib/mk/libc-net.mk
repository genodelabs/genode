LIBC_NET_DIR = $(LIBC_DIR)/lib/libc/net

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

# suppress "warning: ‘strncpy’ specified bound depends on the length of the source argument"
CC_OPT_getaddrinfo := -Wno-stringop-overflow

include $(REP_DIR)/lib/mk/libc-common.inc

INC_DIR += $(LIBC_REP_DIR)/include/libc
INC_DIR += $(LIBC_REP_DIR)/include/libc/sys
INC_DIR += $(LIBC_PORT_DIR)/include/libc/sys

# needed for name6.c, contains res_private.h
INC_DIR += $(LIBC_DIR)/lib/libc/resolv

# needed for net/firewire.h
INC_DIR += $(LIBC_DIR)/sys

vpath %.c $(LIBC_NET_DIR)

nslexer.o: nsparser.c nsparser.c

nslexer.c: nslexer.l
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)flex -P_nsyy -t $< | sed -e '/YY_BUF_SIZE/s/16384/1024/' > $@

vpath nslexer.l $(LIBC_NET_DIR)

nsparser.c: nsparser.y
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)bison -d -p_nsyy $< \
		--defines=nsparser.h --output=$@

vpath nsparser.y $(LIBC_NET_DIR)

CC_CXX_WARN_STRICT =
