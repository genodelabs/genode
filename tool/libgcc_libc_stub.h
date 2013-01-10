/*
 * \brief  Stub for compiling GCC support libraries without libc
 * \author Norman Feske
 * \date   2011-08-31
 *
 * The target components of GCC tool chains (i.e. libsupc++, libgcc_eh, and
 * libstdc++) depend on the presence of libc includes. For this reason, a C
 * library for the target platform is normally regarded as a prerequisite for
 * building a complete tool chain. However, for low-level operating-system
 * code, this prerequisite is not satisfied.
 *
 * There are two traditional solutions to this problem. The first is to leave
 * out those target components from the tool chain and live without full C++
 * support (using '-fno-rtti' and '-fno-exceptions'). Because Genode relies on
 * such C++ features however, this is no option. The other traditional solution
 * is to use a tool chain compiled for a different target platform such as
 * Linux. However, this approach calls for subtle problems because the target
 * components are compiled against glibc and make certain presumptions about
 * the underlying OS environment. E.g., the 'libstdc++' library of a Linux tool
 * chain contains references to glibc's 'stderr' symbol, which does not exist
 * on Genode's libc derived from FreeBSD. More critical assumptions are related
 * to the mechanism used for thread-local storage.
 *
 * This header file overcomes these problems by providing all function
 * prototypes and type declarations that are mandatory for compiling GCC's
 * target components. Using this libc stub, all GCC target components can be
 * built without the need for additional libc support. Of course, for actually
 * using these target components, the target OS has to provide the
 * implementation of a small subset of functions declared herein. On Genode,
 * this subset is provided by the 'cxx' library.
 *
 * The code of the target components expects usual C header file names such as
 * 'stdio.h'. It does not include 'libgcc_libc_stub.h'. By creating symlinks
 * for all those file names pointing to this file, we ensure that this file is
 * always included on the first occurrence of the inclusion of any libc header
 * file. The set of symlinks pointing to this libc stub are created
 * automatically by the 'tool_chain' script.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_STUB_H_
#define _LIBC_STUB_H_

/* used for vararg, comes with GCC */
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/*****************
 ** sys/types.h **
 *****************/

typedef __SIZE_TYPE__ size_t;

#ifndef ssize_t
#define ssize_t long /* xxx 64bit */
#endif

typedef unsigned long  off_t; /* XXX 64bit */

#define pid_t int

typedef unsigned short mode_t; /* XXX 64bit */

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif /* __cplusplus */
#endif /* defined NULL */

typedef long clock_t; /* XXX not on 64bit */

#ifdef _LP64
typedef signed   char  __int8_t;
typedef signed   short __int16_t;
typedef signed   int   __int32_t;
typedef signed   long  __int64_t;
typedef unsigned char  __uint8_t;
typedef unsigned short __uint16_t;
typedef unsigned int   __uint32_t;
typedef unsigned long  __uint64_t;
#else /* _LP64 */
typedef signed   char  __int8_t;
typedef signed   short __int16_t;
typedef signed   long  __int32_t;
typedef unsigned char  __uint8_t;
typedef unsigned short __uint16_t;
typedef unsigned long  __uint32_t;
#ifndef __STRICT_ANSI__
typedef signed   long long __int64_t;
typedef unsigned long long __uint64_t;
#endif /* __STRICT_ANSI__ */
#endif /* _LP64 */

typedef __int64_t  intmax_t;
typedef __int32_t  int_fast8_t;
typedef __int32_t  int_fast16_t;
typedef __int32_t  int_fast32_t;
typedef __int64_t  int_fast64_t;
typedef __int8_t   int_least8_t;
typedef __int16_t  int_least16_t;
typedef __int32_t  int_least32_t;
typedef __int64_t  int_least64_t;
typedef __uint64_t uintmax_t;

#ifdef _LP64
typedef __int64_t  time_t;
typedef __int64_t  intptr_t;
typedef __uint64_t uintptr_t;
#else
typedef __int32_t  time_t;
typedef __int32_t  intptr_t;
typedef __uint32_t uintptr_t;
#endif

typedef __uint32_t uint_fast8_t;
typedef __uint32_t uint_fast16_t;
typedef __uint32_t uint_fast32_t;
typedef __uint64_t uint_fast64_t;
typedef __uint8_t  uint_least8_t;
typedef __uint16_t uint_least16_t;
typedef __uint32_t uint_least32_t;
typedef __uint64_t uint_least64_t;

struct timeval {
	time_t tv_sec;
	long   tv_usec; /* XXX 64bit */
};


/****************
 ** sys/stat.h **
 ****************/

struct stat
{
	unsigned long  st_dev;
	unsigned long  st_ino;
	unsigned short st_mode;
};

#define S_ISREG(m) (((m) & 0170000) == 0100000)


/************
 ** time.h **
 ************/

struct tm {
	int   tm_sec;
	int   tm_min;
	int   tm_hour;
	int   tm_mday;
	int   tm_mon;
	int   tm_year;
	int   tm_wday;
	int   tm_yday;
	int   tm_isdst;
	long  tm_gmtoff;
	char *tm_zone;
};

clock_t clock(void);
double difftime(time_t time1, time_t time0);
struct tm *localtime(const time_t *timep);
char *asctime(const struct tm *tm);
time_t mktime(struct tm *tm);
char *ctime(const time_t *timep);
struct tm *gmtime(const time_t *timep);
time_t time(time_t *t);
size_t strftime(char *s, size_t max, const char *format,
                const struct tm *tm);


/**************
 ** string.h **
 **************/

int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
void *memchr(const void *s, int c, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void *memset(void *s, int c, size_t n);
size_t strcspn(const char *s, const char *reject);
char *strstr(const char *haystack, const char *needle);
size_t strspn(const char *s, const char *accept);
char *strpbrk(const char *s, const char *accept);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

/* for compiling 'libsupc++/del_opvnt.cc' */
void *memmove(void *dest, const void *src, size_t n);
int strcoll(const char *s1, const char *s2);
char *strerror(int errnum);
char *strtok(char *str, const char *delim);
size_t strxfrm(char *dest, const char *src, size_t n);
char *strrchr(const char *s, int c);


/***************
 ** strings.h **
 ***************/

void bcopy(const void *src, void *dest, size_t n);
void bzero(void *s, size_t n);


/**************
 ** stdlib.h **
 **************/

void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void abort(void);
void exit(int);
int atoi(const char *nptr);
void *alloca(size_t size);

/* for compiling 'libsupc++/del_op.cc' */
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
int abs(int j);
long int labs(long int j);
double atof(const char *nptr);
long atol(const char *nptr);
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
void qsort(void *base, size_t nmemb, size_t size,
           int(*compar)(const void *, const void *));
int rand(void);
void srand(unsigned int seed);
int system(const char *command);

#ifdef _ANSIDECL_H
/* special case provided specifically for compiling libiberty's 'strtod.c' */
double strtod(char *nptr, char **endptr);
#else
double strtod(const char *nptr, char **endptr);
#endif

long int strtol(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);
char *getenv(const char *name);
int atexit(void (*function)(void));
void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));


/*************
 ** stdio.h **
 *************/

typedef struct __sFILE { int dummy; } FILE;

extern FILE *__stderrp;
extern FILE *__stdinp;
extern FILE *__stdoutp;

#define stderr __stderrp
#define stdin  __stdinp
#define stdout __stdoutp

/* must not be enum values */
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

enum { _IONBF = 2 };

enum { BUFSIZ = 1024 };

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *fp);
int fprintf(FILE *stream, const char *format, ...);
int fputs(const char *s, FILE *stream);
int sscanf(const char *str, const char *format, ...);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int ferror(FILE *stream);
int sprintf(char *str, const char *format, ...);
FILE *fdopen(int fd, const char *mode);
int fileno(FILE *);

/* for compiling 'libsupc++/vterminate.cc' */
typedef off_t fpos_t;
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fgetc(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, fpos_t *pos);
long ftell(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
void rewind(FILE *stream);
int fputc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
int putc(int c, FILE *stream);
int rename(const char *oldpath, const char *newpath);
int remove(const char *pathname);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int getc(FILE *stream);
int getchar(void);
char *gets(char *s);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
void perror(const char *s);
int printf(const char *format, ...);
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
FILE *tmpfile(void);
char *tmpnam(char *s);
int ungetc(int c, FILE *stream);


/**************
 ** unistd.h **
 **************/

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
pid_t fork(void);
int unlink(const char *pathname);
void _exit(int status);
int link(const char *oldpath, const char *newpath);
pid_t getpid(void);
int pipe(int pipefd[2]);
int dup2(int oldfd, int newfd);
pid_t wait(int *status);
unsigned int sleep(unsigned int seconds);
off_t lseek(int fd, off_t offset, int whence);


/*************
 ** errno.h **
 *************/

#define errno (* __error())

int *__error(void);

/**
 * Error codes corresponding to those of FreeBSD
 */
enum {
	EPERM           =  1,
	ENOENT          =  2,
	ESRCH           =  3,
	EINTR           =  4,
	EIO             =  5,
	ENXIO           =  6,
	E2BIG           =  7,
	ENOEXEC         =  8,
	EBADF           =  9,
	ECHILD          = 10,
	EXDEV           = 18,
	EDEADLK         = 11,
	ENOMEM          = 12,
	EACCES          = 13,
	EFAULT          = 14,
	EBUSY           = 16,
	EEXIST          = 17,
	ENODEV          = 19,
	ENOTDIR         = 20,
	EISDIR          = 21,
	EINVAL          = 22,
	ENFILE          = 23,
	EMFILE          = 24,
	ENOTTY          = 25,
	EFBIG           = 27,
	ENOSPC          = 28,
	ESPIPE          = 29,
	EROFS           = 30,
	EPIPE           = 32,
	EDOM            = 33,
	ERANGE          = 34,
	EAGAIN          = 35,
	EWOULDBLOCK     = EAGAIN,
	EINPROGRESS     = 36,
	EALREADY        = 37,
	ENOTSOCK        = 38,
	EDESTADDRREQ    = 39,
	EMLINK          = 31,
	EMSGSIZE        = 40,
	EPROTOTYPE      = 41,
	ENOPROTOOPT     = 42,
	EPROTONOSUPPORT = 43,
	EOPNOTSUPP      = 45,
	EAFNOSUPPORT    = 47,
	EADDRINUSE      = 48,
	EADDRNOTAVAIL   = 49,
	ENETDOWN        = 50,
	ENETUNREACH     = 51,
	ENETRESET       = 52,
	ECONNABORTED    = 53,
	ECONNRESET      = 54,
	ENOBUFS         = 55,
	EISCONN         = 56,
	ENOTCONN        = 57,
	ETIMEDOUT       = 60,
	ECONNREFUSED    = 61,
	ELOOP           = 62,
	ENAMETOOLONG    = 63,
	EHOSTUNREACH    = 65,
	ENOTEMPTY       = 66,
	ENOLCK          = 77,
	ENOSYS          = 78,
	ENOMSG          = 83,
	EILSEQ          = 86
};


/*************
 ** fcntl.h **
 *************/

enum {
	O_RDONLY = 0x0000,
	O_WRONLY = 0x0001,
	O_RDWR   = 0x0002,
	O_CREAT  = 0x0200,
	O_TRUNC  = 0x0400,
	O_EXCL   = 0x0800
};

enum { F_SETFD = 2 };

enum { FD_CLOEXEC = 1 };

int open(const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ... /* arg */ );


/**************
 ** signal.h **
 **************/

enum { SIGTERM = 15 };

int kill(pid_t pid, int sig);


/*************
 ** ctype.h **
 *************/

int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int toupper(int c);
int tolower(int c);


/**************
 ** locale.h **
 **************/

struct lconv;
char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

enum {
	LC_ALL      = 0,
	LC_COLLATE  = 1,
	LC_CTYPE    = 2,
	LC_MONETARY = 3,
	LC_NUMERIC  = 4,
	LC_TIME     = 5
};


/************
 ** math.h **
 ************/

double acos(double x);
double asin(double x);
double atan(double x);
double atan2(double y, double x);
double ceil(double x);
double cos(double x);
double cosh(double x);
double exp(double x);
double fabs(double x);
double floor(double x);
double fmod(double x, double y);
double frexp(double x, int *exp);
double ldexp(double x, int exp);
double log(double x);
double log10(double x);
double modf(double x, double *iptr);
double pow(double x, double y);
double sin(double x);
double sinh(double x);
double sqrt(double x);
double tan(double x);
double tanh(double x);


/**************
 ** assert.h **
 **************/

#define assert(e) ((void)0)


/***********
 ** elf.h **
 ***********/

/*
 * The following defines and types are solely needed to compile libgcc's
 * 'unwind-dw2-fde-glibc.c' in libc mode. This is needed because Genode's
 * dynamic linker relies on the the "new" exception mechanism, which is not
 * compiled-in when compiling libgcc with the 'inhibit_libc' flag.
 *
 * The following types are loosely based on glibc's 'link.h' and 'elf.h'.
 */

typedef __uint32_t Elf64_Word;
typedef __uint64_t Elf64_Addr;
typedef __uint64_t Elf64_Xword;
typedef __uint64_t Elf64_Off;
typedef __uint16_t Elf64_Half;

typedef struct
{
	Elf64_Word  p_type;
	Elf64_Word  p_flags;
	Elf64_Off   p_offset;
	Elf64_Addr  p_vaddr;
	Elf64_Addr  p_paddr;
	Elf64_Xword p_filesz;
	Elf64_Xword p_memsz;
	Elf64_Xword p_align;
} Elf64_Phdr;

typedef __uint32_t Elf32_Word;
typedef __uint32_t Elf32_Addr;
typedef __uint64_t Elf32_Xword;
typedef __uint32_t Elf32_Off;
typedef __uint16_t Elf32_Half;

typedef struct
{
	Elf32_Word p_type;
	Elf32_Off  p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
} Elf32_Phdr;

#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_LOOS    0x60000000


/************
 ** link.h **
 ************/

/* definitions according to glibc */

#ifdef _LP64
#define ElfW(type) Elf64_##type
#else
#define ElfW(type) Elf32_##type
#endif /* _LP64 */

struct dl_phdr_info
{
	ElfW(Addr) dlpi_addr;
	const char *dlpi_name;
	const ElfW(Phdr) *dlpi_phdr;
	ElfW(Half) dlpi_phnum;
	unsigned long long int dlpi_adds;
	unsigned long long int dlpi_subs;
	size_t dlpi_tls_modid;
	void *dlpi_tls_data;
};

extern int dl_iterate_phdr(int (*__callback) (struct dl_phdr_info *,
                                              size_t, void *), void *__data);


/****************
 ** features.h **
 ****************/

/* let check at the beginning of 'gcc/unwind-dw2-fde-glibc.c' pass */
#define __GLIBC__ 99


#ifdef __cplusplus
}
#endif

#endif /* _LIBC_STUB_H_ */

