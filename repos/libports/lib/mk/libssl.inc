LIBSSL_PORT_DIR = $(call select_from_ports,openssl)

LIBS += libc libcrypto

SRC_C = s2_meth.c   s2_srvr.c s2_clnt.c  s2_lib.c  s2_enc.c s2_pkt.c \
        s3_meth.c   s3_srvr.c s3_clnt.c  s3_lib.c  s3_enc.c s3_pkt.c s3_both.c s3_cbc.c \
        s23_meth.c s23_srvr.c s23_clnt.c s23_lib.c          s23_pkt.c \
        t1_meth.c   t1_srvr.c t1_clnt.c  t1_lib.c  t1_enc.c \
        d1_meth.c   d1_srvr.c d1_clnt.c  d1_lib.c  d1_pkt.c \
        d1_both.c d1_enc.c d1_srtp.c \
        ssl_lib.c ssl_err2.c ssl_cert.c ssl_sess.c \
        ssl_ciph.c ssl_stat.c ssl_rsa.c \
        ssl_asn1.c ssl_txt.c ssl_algs.c \
        bio_ssl.c ssl_err.c kssl.c tls_srp.c t1_reneg.c

INC_DIR += $(LIBSSL_PORT_DIR)/include/openssl
INC_DIR += $(LIBSSL_PORT_DIR)/src/lib/openssl
INC_DIR += $(LIBSSL_PORT_DIR)/src/lib/openssl/crypto
INC_DIR += $(LIBSSL_PORT_DIR)/include

vpath %.c $(LIBSSL_PORT_DIR)/src/lib/openssl/ssl

SHARED_LIB = yes
