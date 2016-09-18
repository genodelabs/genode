/*
 * \brief  Memory manipulation utilities
 * \author Emery Hemingway
 * \date   2017-08-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__INCLUDE__STRING_H__
#define __LWIP__INCLUDE__STRING_H__

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *dst, const void *src, size_t len);
void *memset(void *b, int c, size_t len);

size_t strlen(const char *s);

int memcmp(const void *b1, const void *b2, size_t len);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __LWIP__INCLUDE__STRING_H__ */