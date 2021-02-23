/*
 * Override the lib'c wchar.h header to work around the type conflict between
 * the libc's wint_t (defined as signed integer) and GCC's builtin type
 * (defined as unsigned integer). E.g.,
 *
 * COMPILE  Runtime/r3/posix/utf8-posix.o
 *  .../libc/wctype.h:61:5: error: declaration of ‘int iswalnum(wint_t)’
 *            conflicts with built-in declaration ‘int iswalnum(unsigned int)’
 *            [-Werror=builtin-declaration-mismatch]
 */

#ifndef _LIBC__WCHAR_H_
#define _LIBC__WCHAR_H_

#include <sys/_types.h>

typedef __mbstate_t mbstate_t;

typedef unsigned int wint_t;

#endif /* _LIBC__WCHAR_H_ */
