include $(REP_DIR)/ports/libssh.inc

LIBSSH_DIR = $(REP_DIR)/contrib/$(LIBSSH)

SRC_C = \
        agent.c \
        auth.c \
        base64.c \
        buffer.c \
        callbacks.c \
        channels.c \
        client.c \
        config.c \
        connect.c \
        crc32.c \
        crypt.c \
        dh.c \
        error.c \
        getpass.c \
        gcrypt_missing.c \
        gzip.c \
        init.c \
        kex.c \
        keyfiles.c \
        keys.c \
        known_hosts.c \
        legacy.c \
        libcrypto.c \
        libgcrypt.c \
        log.c \
        match.c \
        messages.c \
        misc.c \
        options.c \
        packet.c \
        pcap.c \
        pki.c \
        poll.c \
        session.c \
        scp.c \
        socket.c \
        string.c \
        threads.c \
        wrapper.c

#INC_DIR += $(LIBSSH_DIR)
INC_DIR += $(REP_DIR)/src/lib/libssh

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc zlib libcrypto

SHARED_LIB = yes

vpath %.c $(LIBSSH_DIR)/src
