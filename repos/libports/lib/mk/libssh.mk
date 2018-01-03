LIBSSH_PORT_DIR := $(call select_from_ports,libssh)

SRC_C = \
        agent.c \
        auth.c \
        base64.c \
        bignum.c \
        bind.c \
        buffer.c \
        callbacks.c \
        channels.c \
        client.c \
        config.c \
        connect.c \
        curve25519.c \
        dh.c \
        ecdh.c \
        error.c \
        getpass.c \
        gzip.c \
        init.c \
        kex.c \
        known_hosts.c \
        legacy.c \
        libcrypto.c \
        log.c \
        match.c \
        messages.c \
        misc.c \
        options.c \
        packet.c \
        packet_cb.c \
        packet_crypt.c \
        pcap.c \
        pki.c \
        pki_container_openssh.c \
        pki_crypto.c \
        pki_ed25519.c \
        poll.c \
        scp.c \
        server.c \
        session.c \
        sftp.c \
        sftpserver.c \
        socket.c \
        string.c \
        threads.c \
        wrapper.c

# external/
SRC_C += \
         bcrypt_pbkdf.c \
         blowfish.c \
         curve25519_ref.c \
         ed25519.c \
         fe25519.c \
         ge25519.c \
         sc25519.c

INC_DIR += $(LIBSSH_PORT_DIR)/include
INC_DIR += $(REP_DIR)/src/lib/libssh

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc zlib libcrypto

SHARED_LIB = yes

vpath %.c $(LIBSSH_PORT_DIR)/src/lib/libssh/src
vpath %.c $(LIBSSH_PORT_DIR)/src/lib/libssh/src/external

CC_CXX_WARN_STRICT =
