CURL_PORT_DIR := $(call select_from_ports,curl)
LIB_SRC_DIR   := $(CURL_PORT_DIR)/src/lib/curl/lib

SRC_C = \
	altsvc.c \
	amigaos.c \
	asyn-ares.c \
	asyn-thread.c \
	base64.c \
	conncache.c \
	connect.c \
	content_encoding.c \
	cookie.c \
	curl_addrinfo.c \
	curl_ctype.c \
	curl_des.c \
	curl_endian.c \
	curl_fnmatch.c \
	curl_get_line.c \
	curl_gethostname.c \
	curl_gssapi.c \
	curl_memrchr.c \
	curl_multibyte.c \
	curl_ntlm_core.c \
	curl_ntlm_wb.c \
	curl_path.c \
	curl_range.c \
	curl_rtmp.c \
	curl_sasl.c \
	curl_sspi.c \
	curl_threads.c \
	dict.c \
	doh.c \
	dotdot.c \
	easy.c \
	escape.c \
	file.c \
	fileinfo.c \
	formdata.c \
	ftp.c \
	ftplistparser.c \
	getenv.c \
	getinfo.c \
	gopher.c \
	hash.c \
	hmac.c \
	hostasyn.c \
	hostcheck.c \
	hostip.c \
	hostip4.c \
	hostip6.c \
	hostsyn.c \
	http.c \
	http2.c \
	http_chunks.c \
	http_digest.c \
	http_negotiate.c \
	http_ntlm.c \
	http_proxy.c \
	idn_win32.c \
	if2ip.c \
	imap.c \
	inet_ntop.c \
	inet_pton.c \
	krb5.c \
	ldap.c \
	llist.c \
	md4.c \
	md5.c \
	memdebug.c \
	mime.c \
	mprintf.c \
	mqtt.c \
	multi.c \
	netrc.c \
	non-ascii.c \
	nonblock.c \
	openldap.c \
	parsedate.c \
	pingpong.c \
	pop3.c \
	progress.c \
	psl.c \
	rand.c \
	rename.c \
	rtsp.c \
	security.c \
	select.c \
	sendf.c \
	setopt.c \
	sha256.c \
	share.c \
	slist.c \
	smb.c \
	smtp.c \
	socketpair.c \
	socks.c \
	socks_gssapi.c \
	socks_sspi.c \
	speedcheck.c \
	splay.c \
	strcase.c \
	strdup.c \
	strerror.c \
	strtok.c \
	strtoofft.c \
	system_win32.c \
	telnet.c \
	tftp.c \
	timeval.c \
	transfer.c \
	url.c \
	urlapi.c \
	vauth/cleartext.c \
	vauth/cram.c \
	vauth/digest.c \
	vauth/digest_sspi.c \
	vauth/krb5_gssapi.c \
	vauth/krb5_sspi.c \
	vauth/ntlm.c \
	vauth/ntlm_sspi.c \
	vauth/oauth2.c \
	vauth/spnego_gssapi.c \
	vauth/spnego_sspi.c \
	vauth/vauth.c \
	version.c \
	vquic/ngtcp2.c \
	vquic/quiche.c \
	vssh/libssh.c \
	vssh/libssh2.c \
	vssh/wolfssh.c \
	vtls/bearssl.c \
	vtls/gskit.c \
	vtls/gtls.c \
	vtls/mbedtls.c \
	vtls/mbedtls_threadlock.c \
	vtls/mesalink.c \
	vtls/nss.c \
	vtls/openssl.c \
	vtls/schannel.c \
	vtls/schannel_verify.c \
	vtls/sectransp.c \
	vtls/vtls.c \
	vtls/wolfssl.c \
	warnless.c \
	wildcard.c \
	x509asn1.c \
	# end of SRC_C

CC_OPT += -DHAVE_CONFIG_H -DBUILDING_LIBCURL=1

LIBS += libc zlib libssl libssh

include $(call select_from_repositories,lib/import/import-curl.mk)

INC_DIR += $(LIB_SRC_DIR)

SHARED_LIB = yes

vpath %.c $(LIB_SRC_DIR)

# vi: set ft=make :
