TARGET = openvpn

LIBS += libc libc_pipe libc_lwip_nic_dhcp \
        libcrypto libssl

OPENVPN_PORT_DIR := $(call select_from_ports,openvpn)
OPENVPN_DIR      := $(OPENVPN_PORT_DIR)/src/app/openvpn

SRC_C_compat := compat-dirname.c \
                compat-basename.c \
                compat-gettimeofday.c \
                compat-daemon.c \
                compat-inet_ntop.c \
                compat-inet_pton.c

SRC_C_openvpn := base64.c \
                 buffer.c \
                 clinat.c \
                 console.c \
                 crypto.c \
                 crypto_openssl.c \
                 cryptoapi.c \
                 dhcp.c \
                 error.c \
                 event.c \
                 fdmisc.c \
                 forward.c \
                 fragment.c \
                 gremlin.c \
                 helper.c \
                 httpdigest.c \
                 init.c \
                 interval.c \
                 list.c \
                 lladdr.c \
                 lzo.c \
                 manage.c \
                 mbuf.c \
                 misc.c \
                 mroute.c \
                 mss.c \
                 mstats.c \
                 mtcp.c \
                 mtu.c \
                 mudp.c \
                 multi.c \
                 ntlm.c \
                 occ.c \
                 openvpn.c \
                 options.c \
                 otime.c \
                 packet_id.c \
                 perf.c \
                 pf.c \
                 ping.c \
                 pkcs11.c \
                 pkcs11_openssl.c \
                 platform.c \
                 plugin.c \
                 pool.c \
                 proto.c \
                 proxy.c \
                 ps.c \
                 push.c \
                 reliable.c \
                 route.c \
                 schedule.c \
                 session_id.c \
                 shaper.c \
                 sig.c \
                 socket.c \
                 socks.c \
                 ssl.c \
                 ssl_openssl.c \
                 ssl_verify.c \
                 ssl_verify_openssl.c \
                 status.c

SRC_CC = main.cc tun_genode.cc

CC_CXX_OPT += -fpermissive

# too much to cope with...
CC_WARN =

SRC_C := $(SRC_C_compat) $(SRC_C_openvpn) dummies.c

CC_OPT += -DHAVE_CONFIG_H -DSELECT_PREFERRED_OVER_POLL

INC_DIR += $(OPENVPN_DIR)/include
INC_DIR += $(OPENVPN_DIR)/src/compat
INC_DIR += $(OPENVPN_DIR)/src/openvpn

# find 'config.h'
ifeq ($(filter-out $(SPECS),32bit),)
	TARGET_CPUBIT=32bit
else ifeq ($(filter-out $(SPECS),64bit),)
	TARGET_CPUBIT=64bit
endif
INC_DIR += $(REP_DIR)/src/app/openvpn/spec/$(TARGET_CPUBIT)
INC_DIR += $(REP_DIR)/src/app/openvpn/

vpath compat-%.c $(OPENVPN_DIR)/src/compat
vpath %.c        $(OPENVPN_DIR)/src/openvpn
vpath %.cc       $(REP_DIR)/src/app/openvpn
