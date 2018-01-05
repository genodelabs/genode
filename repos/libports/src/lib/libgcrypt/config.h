#define _GCRYPT_CONFIG_H_INCLUDED 1

#define HAVE_STDINT_H      1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_GETPID        1
#define HAVE_CLOCK         1

#ifdef __LP64__
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT   4
#define SIZEOF_UNSIGNED_LONG  8
#else
#define SIZEOF_UNSIGNED_SHORT     2
#define SIZEOF_UNSIGNED_INT       4
#define SIZEOF_UNSIGNED_LONG      4
#define SIZEOF_UNSIGNED_LONG_LONG 8
#endif

#define USE_SHA1   1
#define USE_SHA256 1
#define USE_RSA    1
