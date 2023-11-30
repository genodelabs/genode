LIGHTTPD_DIR := $(call select_from_ports,lighttpd)/src/app/lighttpd

FILTER_OUT := lempar.c lighttpd-angel.c lemon.c \
              mod_accesslog.c \
              mod_alias.c \
              mod_authn_dbi.c \
              mod_authn_file.c \
              mod_authn_gssapi.c \
              mod_authn_ldap.c \
              mod_authn_mysql.c \
              mod_authn_pam.c \
              mod_authn_sasl.c \
              mod_cgi.c \
              mod_cml.c \
              mod_cml_funcs.c \
              mod_cml_lua.c \
              mod_compress.c \
              mod_deflate.c \
              mod_evasive.c \
              mod_evhost.c \
              mod_expire.c \
              mod_extforward.c \
              mod_fastcgi.c \
              mod_flv_streaming.c \
              mod_geoip.c \
              mod_gnutls.c \
              mod_magnet.c \
              mod_magnet_cache.c \
              mod_maxminddb.c \
              mod_mbedtls.c \
              mod_mysql_vhost.c \
              mod_nss.c \
              mod_proxy.c \
              mod_redirect.c \
              mod_rewrite.c \
              mod_rrdtool.c \
              mod_scgi.c \
              mod_secdownload.c \
              mod_setenv.c \
              mod_simple_vhost.c \
              mod_skeleton.c \
              mod_sockproxy.c \
              mod_ssi.c \
              mod_ssi_expr.c \
              mod_ssi_exprparser.c \
              mod_status.c \
              mod_trigger_b4_dl.c \
              mod_uploadprogress.c \
              mod_userdir.c \
              mod_usertrack.c \
              mod_vhostdb.c \
              mod_vhostdb_dbi.c \
              mod_vhostdb_ldap.c \
              mod_vhostdb_mysql.c \
              mod_vhostdb_pgsql.c \
              mod_webdav.c \
              mod_wolfssl.c \
              mod_wstunnel.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIGHTTPD_DIR)/src/*.c)))

vpath %.c $(LIGHTTPD_DIR)/src

# read version information from configure script
LIGHTTPD_VERSION = $(subst ., ,$(shell cat $(LIGHTTPD_DIR)/configure |\
                                        grep "PACKAGE_VERSION=" |\
                                        sed "s/^.*='//;s/'//"))

LIGHTTPD_MAIN  = $(word 1,$(LIGHTTPD_VERSION))
LIGHTTPD_MAJOR = $(word 2,$(LIGHTTPD_VERSION))
LIGHTTPD_MINOR = $(word 3,$(LIGHTTPD_VERSION))

CC_OPT += -DHAVE_SOCKLEN_T -DHAVE_SYSLOG_H -DHAVE_STDINT_H -DUSE_POLL
CC_OPT += -DHAVE_SYS_WAIT_H -DHAVE_SYS_UN_H -DHAVE_MMAP -DHAVE_SYS_MMAN_H -DHAVE_SELECT
CC_OPT += -DHAVE_WRITEV -DUSE_WRITEV
CC_OPT += -DHAVE_TIMEGM -DHAVE_UNISTD_H -DHAVE_SYS_TIME_H
CC_OPT += -DSBIN_DIR="\"/sbin\""
CC_OPT += -DPACKAGE_NAME="\"lighttpd\""
CC_OPT += -DLIGHTTPD_VERSION_ID='($(LIGHTTPD_MAIN) << 16 | $(LIGHTTPD_MAJOR) << 8 | $(LIGHTTPD_MINOR))'
CC_OPT += -DPACKAGE_VERSION="\"$(LIGHTTPD_MAIN).$(LIGHTTPD_MAJOR).$(LIGHTTPD_MINOR)\""
CC_OPT += -DLIBRARY_DIR="\"/lib\""
CC_OPT += -DLIGHTTPD_STATIC
CC_OPT += -DHAVE_LIBSSL -DUSE_OPENSSL -DUSE_OPENSSL_CRYPTO


CC_WARN = -Wall -Wno-unused-variable -Wno-unused-function

INC_DIR += $(PRG_DIR)
INC_DIR += $(LIGHTTPD_DIR)/src

LIBS += posix
LIBS += zlib
LIBS += libssl
