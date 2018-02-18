//#define ANCHOR_DEBUG 1
//#define DAEMON_DEBUG 1
#define DNSSEC_ROADBLOCK_AVOIDANCE 1
#define DRAFT_RRTYPES 1
#define EDNS_COOKIES 1
#define EDNS_COOKIE_OPCODE 10
#define EDNS_COOKIE_ROLLOVER_TIME (24 * 60 * 60)
#define EDNS_PADDING_OPCODE 12
#define GETDNS_FN_HOSTS "/etc/hosts"
#define GETDNS_FN_RESOLVCONF "/etc/resolv.conf"
#define HAVE_ARPA_INET_H 1
#define HAVE_ATTR_FORMAT 1
#define HAVE_ATTR_UNUSED 1
#define HAVE_DECL_ARC4RANDOM 0
#define HAVE_DECL_ARC4RANDOM_UNIFORM 0
#define HAVE_DECL_INET_NTOP 0
#define HAVE_DECL_INET_PTON 0
#define HAVE_DECL_NID_SECP384R1 1
#define HAVE_DECL_NID_X9_62_PRIME256V1 1
#define HAVE_DECL_SK_SSL_COMP_POP_FREE 1
#define HAVE_DECL_SSL_COMP_GET_COMPRESSION_METHODS 1
//#define HAVE_DECL_SSL_CTX_SET1_CURVES_LIST 1
#define HAVE_DECL_SSL_CTX_SET_ECDH_AUTO 1
//#define HAVE_DECL_SSL_SET1_CURVES_LIST 1
#define HAVE_DECL_STRLCPY 0
#define HAVE_DLFCN_H 1
#define HAVE_ENDIAN_H 1
#define HAVE_ENGINE_LOAD_CRYPTODEV 1
#define HAVE_EVP_DSS1 1
#define HAVE_EVP_MD5 1
#define HAVE_EVP_PKEY_BASE_ID 1
#define HAVE_EVP_PKEY_KEYGEN 1
#define HAVE_EVP_SHA1 1
#define HAVE_EVP_SHA224 1
#define HAVE_EVP_SHA256 1
#define HAVE_EVP_SHA384 1
#define HAVE_EVP_SHA512 1
#define HAVE_FCNTL 1
#define HAVE_FIPS_MODE 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETAUXVAL 1
#define HAVE_HMAC_UPDATE 1
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIBYAML 1
#define HAVE_LIMITS_H 1
#define HAVE_MEMORY_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_OPENSSL_BN_H 1
#define HAVE_OPENSSL_CONFIG 1
#define HAVE_OPENSSL_CONF_H 1
#define HAVE_OPENSSL_DSA_H 1
#define HAVE_OPENSSL_ENGINE_H 1
#define HAVE_OPENSSL_ERR_H 1
#define HAVE_OPENSSL_RAND_H 1
#define HAVE_OPENSSL_RSA_H 1
#define HAVE_OPENSSL_SSL_H 1
//#define HAVE_POLL_H 1
//#define HAVE_PTHREAD 1
#define HAVE_SIGADDSET 1
#define HAVE_SIGEMPTYSET 1
#define HAVE_SIGFILLSET 1
#define HAVE_SIGNAL_H 1
#define HAVE_SIGSET_T 1
#define HAVE_SSL /**/
//#define HAVE_SSL_HN_AUTH 1
// FIXME
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRPTIME 1
//#define HAVE_SYS_POLL_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_SYSCTL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_TLS_v1_2 1
#define HAVE_UNISTD_H 1
#define HAVE_U_CHAR 1
//#define HAVE_X509_CHECK_HOST 1
#define HAVE___FUNC__ 1
#define LT_OBJDIR ".libs/"
#define MAXIMUM_UPSTREAM_OPTION_SPACE 3000
#define MAX_CNAME_REFERRALS 100
#define PACKAGE_BUGREPORT "team@getdnsapi.net"
#define PACKAGE_NAME "getdns"
#define PACKAGE_STRING "getdns 1.4.0"
#define PACKAGE_TARNAME "getdns"
#define PACKAGE_URL "https://getdnsapi.net"
#define PACKAGE_VERSION "1.4.0"
//#define REQ_DEBUG 1
#define RETSIGTYPE void
//#define SCHED_DEBUG 1
//#define SEC_DEBUG 1
//#define SERVER_DEBUG 1
#define STDC_HEADERS 1
#define STRPTIME_WORKS 1
#define STUBBY_PACKAGE "stubby"
#define STUBBY_PACKAGE_STRING ""
//#define STUB_DEBUG 1
#define STUB_NATIVE_DNSSEC 1
#define SYSCONFDIR sysconfdir
#define TRUST_ANCHOR_FILE "/getdns-root.key"
#define USE_DANESSL 1
#define USE_DSA 1
#define USE_ECDSA 1
#define USE_GOST 1
#define USE_SHA1 1
#define USE_SHA2 1
#ifdef HAVE___FUNC__
#define __FUNC__ __func__
#else
#define __FUNC__ __FUNCTION__
#endif
#ifdef GETDNS_ON_WINDOWS
# ifndef FD_SETSIZE
#  define FD_SETSIZE 1024
# endif
# ifndef WINVER
#  define WINVER 0x0600 // 0x0502
# endif
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0600 // 0x0502
# endif
# ifdef HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
# endif
# ifdef _MSC_VER
#  if _MSC_VER >= 1800
#   define PRIsz "zu"
#  else
#   define PRIsz "Iu"
#  endif
# else
#  define PRIsz "Iu"
# endif
# ifdef HAVE_WINSOCK2_H
#  include <winsock2.h>
# endif
# ifdef HAVE_WINSOCK2_H
#  define FD_SET_T (u_int)
# else
#  define FD_SET_T 
# endif
 /* Windows wants us to use _strdup instead of strdup */
# ifndef strdup
#  define strdup _strdup
# endif
#else
# define PRIsz "zu"
#endif
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#if !defined(HAVE_STRLCPY) || !HAVE_DECL_STRLCPY || !defined(strlcpy)
size_t strlcpy(char *dst, const char *src, size_t siz);
#else
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#endif
#if !defined(HAVE_ARC4RANDOM) || !HAVE_DECL_ARC4RANDOM
uint32_t arc4random(void);
#endif
#if !defined(HAVE_ARC4RANDOM_UNIFORM) || !HAVE_DECL_ARC4RANDOM_UNIFORM 
uint32_t arc4random_uniform(uint32_t upper_bound);
#endif
#ifndef HAVE_ARC4RANDOM
void explicit_bzero(void* buf, size_t len);
int getentropy(void* buf, size_t len);
void arc4random_buf(void* buf, size_t n);
void _ARC4_LOCK(void);
void _ARC4_UNLOCK(void);
#endif
#ifndef HAVE_DECL_INET_PTON
int inet_pton(int af, const char* src, void* dst);
#endif /* HAVE_INET_PTON */
#ifndef HAVE_DECL_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif
#ifdef USE_WINSOCK
# ifndef  _CUSTOM_VSNPRINTF
#  define _CUSTOM_VSNPRINTF
static inline int _gldns_custom_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{ int r = vsnprintf(str, size, format, ap); return r == -1 ? _vscprintf(format, ap) : r; }
#  define vsnprintf _gldns_custom_vsnprintf
# endif
#endif
#ifdef __cplusplus
}
#endif
#define USE_GLDNS 1
#ifdef HAVE_SSL
#  define GLDNS_BUILD_CONFIG_HAVE_SSL 1
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_OPENSSL_SSL_H
#include <openssl/ssl.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_LIMITS_H
#include <sys/limits.h>
#endif
#ifdef PATH_MAX
#define _GETDNS_PATH_MAX PATH_MAX
#else
#define _GETDNS_PATH_MAX 2048
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifdef HAVE_ATTR_FORMAT
#  define ATTR_FORMAT(archetype, string_index, first_to_check) \
    __attribute__ ((format (archetype, string_index, first_to_check)))
#else /* !HAVE_ATTR_FORMAT */
#  define ATTR_FORMAT(archetype, string_index, first_to_check) /* empty */
#endif /* !HAVE_ATTR_FORMAT */
#if defined(DOXYGEN)
#  define ATTR_UNUSED(x)  x
#elif defined(__cplusplus)
#  define ATTR_UNUSED(x)
#elif defined(HAVE_ATTR_UNUSED)
#  define ATTR_UNUSED(x)  x __attribute__((unused))
#else /* !HAVE_ATTR_UNUSED */
#  define ATTR_UNUSED(x)  x
#endif /* !HAVE_ATTR_UNUSED */
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if !defined(HAVE_STRPTIME) || !defined(STRPTIME_WORKS)
#define strptime unbound_strptime
struct tm;
char *strptime(const char *s, const char *format, struct tm *tm);
#endif
#if !defined(HAVE_SIGSET_T) && defined(HAVE__SIGSET_T)
typedef _sigset_t sigset_t;
#endif
#if !defined(HAVE_SIGEMPTYSET)
#  define sigemptyset(pset)    (*(pset) = 0)
#endif
#if !defined(HAVE_SIGFILLSET)
#  define sigfillset(pset)     (*(pset) = (sigset_t)-1)
#endif
#if !defined(HAVE_SIGADDSET)
#  define sigaddset(pset, num) (*(pset) |= (1L<<(num)))
#endif
#ifdef HAVE_LIBUNBOUND
# include <unbound.h>
# ifdef HAVE_UNBOUND_EVENT_H
#  include <unbound-event.h>
# else
#  ifdef HAVE_UNBOUND_EVENT_API
#   ifndef _UB_EVENT_PRIMITIVES
#    define _UB_EVENT_PRIMITIVES
struct ub_event_base;
struct ub_ctx* ub_ctx_create_ub_event(struct ub_event_base* base);
typedef void (*ub_event_callback_t)(void*, int, void*, int, int, char*);
int ub_resolve_event(struct ub_ctx* ctx, const char* name, int rrtype, 
        int rrclass, void* mydata, ub_event_callback_t callback, int* async_id);
#   endif
#  endif
# endif
#endif
#ifdef __cplusplus
}
#endif
