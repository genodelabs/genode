/*
 * \brief  Stub for compiling GCC support libraries without libc
 * \author Norman Feske
 * \date   2011-08-31
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LINK_H_
#define _LINK_H_

#include <stddef.h>

/*****************
 ** sys/types.h **
 *****************/

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

#endif /* _LINK_H_ */
