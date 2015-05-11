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
