/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/********************
 ** linux/string.h **
 ********************/
#undef memcpy

void  *memcpy(void *d, const void *s, size_t n);
void  *memset(void *s, int c, size_t n);
int    memcmp(const void *, const void *, size_t);
void  *memscan(void *addr, int c, size_t size);
char  *strcat(char *dest, const char *src);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *cs, const char *ct, size_t count);
char  *strcpy(char *to, const char *from);
char  *strncpy(char *, const char *, size_t);
char  *strchr(const char *, int);
char  *strrchr(const char *,int);
size_t strlcat(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
char * strsep(char **,const char *);
char *strstr(const char *, const char *);
char  *kstrdup(const char *s, gfp_t gfp);
void  *kmemdup(const void *src, size_t len, gfp_t gfp);
void  *memmove(void *, const void *, size_t);
void * memchr(const void *, int, size_t);
