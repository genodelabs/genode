LIBSSL     = libssl-1.0.0
LIBSSL_DIR = $(REP_DIR)/contrib/openssl-1.0.1c

#
# ARM is not supported currently (needs testing)
#
REQUIRES = x86

LIBS      += libc libcrypto

SRC_C = s2_meth.c   s2_srvr.c s2_clnt.c  s2_lib.c  s2_enc.c s2_pkt.c \
        s3_meth.c   s3_srvr.c s3_clnt.c  s3_lib.c  s3_enc.c s3_pkt.c s3_both.c \
        s23_meth.c s23_srvr.c s23_clnt.c s23_lib.c          s23_pkt.c \
        t1_meth.c   t1_srvr.c t1_clnt.c  t1_lib.c  t1_enc.c \
        d1_meth.c   d1_srvr.c d1_clnt.c  d1_lib.c  d1_pkt.c \
        d1_both.c d1_enc.c d1_srtp.c \
        ssl_lib.c ssl_err2.c ssl_cert.c ssl_sess.c \
        ssl_ciph.c ssl_stat.c ssl_rsa.c \
        ssl_asn1.c ssl_txt.c ssl_algs.c \
        bio_ssl.c ssl_err.c kssl.c tls_srp.c t1_reneg.c

INC_DIR += $(LIBSSL_DIR)/
INC_DIR += $(LIBSSL_DIR)/crypto

ifeq ($(filter-out $(SPECS),x86_32),)
TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
TARGET_CPUARCH=x86_64
endif

INC_DIR += $(REP_DIR)/src/lib/openssl/$(TARGET_CPUARCH)/


vpath %.c $(LIBSSL_DIR)/ssl

SHARED_LIB = yes
