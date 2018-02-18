/* hand-tweaked automess */
#define HAVE_ASSERT_H 1
#define HAVE_ATTR_FORMAT 1
#define HAVE_ATTR_UNUSED 1
#define HAVE_GETDNS_GETDNS_EXTRA_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIBGETDNS 1
#define HAVE_LIBYAML 1
#define HAVE_MEMORY_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define PACKAGE "stubby"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "Stubby"
#define PACKAGE_STRING "Stubby"
#define PACKAGE_TARNAME "stubby"
#define PACKAGE_URL ""
#define PACKAGE_VERSION ""
#define STDC_HEADERS 1
#define VERSION ""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#ifdef HAVE_ATTR_FORMAT
# define ATTR_FORMAT(archetype, string_index, first_to_check) \
    __attribute__ ((format (archetype, string_index, first_to_check)))
#else /* !HAVE_ATTR_FORMAT */
# define ATTR_FORMAT(archetype, string_index, first_to_check) /* empty */
#endif /* !HAVE_ATTR_FORMAT */
#if defined(__cplusplus)
#  define ATTR_UNUSED(x)
#elif defined(HAVE_ATTR_UNUSED)
#  define ATTR_UNUSED(x)  x __attribute__((unused))
#else /* !HAVE_ATTR_UNUSED */
#  define ATTR_UNUSED(x)  x
#endif /* !HAVE_ATTR_UNUSED */
#ifndef HAVE_GETDNS_YAML2DICT
# define USE_YAML_CONFIG 1
#endif
#define yaml_string_to_json_string        stubby_yaml_string_to_json_string
#define PRIsz "zu"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#define HAVE_NETDB_H
#define HAVE_TIME_H
