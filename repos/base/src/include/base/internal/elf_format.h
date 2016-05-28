/*
 * \brief  ELF binary definition from GNU C library
 * \author Christian Helmuth
 * \date   2006-05-04
 */

/*
 * This file was slightly modified and stripped down. Original copyright
 * header follows:
 *
 * This file defines standard ELF types, structures, and macros.
 * Copyright (C) 1995-2003, 2004, 2005 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the GNU C Library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

#ifndef _INCLUDE__BASE__INTERNAL__ELF_FORMAT_H_
#define _INCLUDE__BASE__INTERNAL__ELF_FORMAT_H_

extern "C" {

/* standard ELF types.  */
#include <base/stdint.h>

/* type for a 16-bit quantity.  */
typedef genode_uint16_t Elf32_Half;
typedef genode_uint16_t Elf64_Half;

/* types for signed and unsigned 32-bit quantities */
typedef genode_uint32_t Elf32_Word;
typedef genode_int32_t  Elf32_Sword;
typedef genode_uint32_t Elf64_Word;
typedef genode_int32_t  Elf64_Sword;

/* types for signed and unsigned 64-bit quantities */
typedef genode_uint64_t Elf32_Xword;
typedef genode_int64_t  Elf32_Sxword;
typedef genode_uint64_t Elf64_Xword;
typedef genode_int64_t  Elf64_Sxword;

/* type of addresses */
typedef genode_uint32_t Elf32_Addr;
typedef genode_uint64_t Elf64_Addr;

/* type of file offsets */
typedef genode_uint32_t Elf32_Off;
typedef genode_uint64_t Elf64_Off;

/* type for section indices, which are 16-bit quantities */
typedef genode_uint16_t Elf32_Section;
typedef genode_uint16_t Elf64_Section;

/* type for version symbol information */
typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;


/**
 * The ELF file header. This appears at the start of every ELF file.
 */
enum { EI_NIDENT = 16 };

typedef struct
{
	unsigned char e_ident[EI_NIDENT];   /* Magic number and other info */
	Elf32_Half    e_type;               /* Object file type */
	Elf32_Half    e_machine;            /* Architecture */
	Elf32_Word    e_version;            /* Object file version */
	Elf32_Addr    e_entry;              /* Entry point virtual address */
	Elf32_Off     e_phoff;              /* Program header table file offset */
	Elf32_Off     e_shoff;              /* Section header table file offset */
	Elf32_Word    e_flags;              /* Processor-specific flags */
	Elf32_Half    e_ehsize;             /* ELF header size in bytes */
	Elf32_Half    e_phentsize;          /* Program header table entry size */
	Elf32_Half    e_phnum;              /* Program header table entry count */
	Elf32_Half    e_shentsize;          /* Section header table entry size */
	Elf32_Half    e_shnum;              /* Section header table entry count */
	Elf32_Half    e_shstrndx;           /* Section header string table index */
} Elf32_Ehdr;

typedef struct
{
	unsigned char e_ident[EI_NIDENT];   /* magic number and other info       */
	Elf64_Half    e_type;               /* object file type                  */
	Elf64_Half    e_machine;            /* architecture                      */
	Elf64_Word    e_version;            /* object file version               */
	Elf64_Addr    e_entry;              /* entry point virtual address       */
	Elf64_Off     e_phoff;              /* program header table file offset  */
	Elf64_Off     e_shoff;              /* section header table file offset  */
	Elf64_Word    e_flags;              /* processor-specific flags          */
	Elf64_Half    e_ehsize;             /* eLF header size in bytes          */
	Elf64_Half    e_phentsize;          /* program header table entry size   */
	Elf64_Half    e_phnum;              /* program header table entry count  */
	Elf64_Half    e_shentsize;          /* section header table entry size   */
	Elf64_Half    e_shnum;              /* section header table entry count  */
	Elf64_Half    e_shstrndx;           /* section header string table index */
} Elf64_Ehdr;

/**
 * Fields in the e_ident array.  The EI_* macros are indices into the
 * array.  The macros under each EI_* macro are the values the byte
 * may have.
 */
enum {
	EI_MAG0 = 0,              /* file identification byte 0 index */
	ELFMAG0 = 0x7f,           /* magic number byte 0 */
	
	EI_MAG1 = 1,              /* file identification byte 1 index */
	ELFMAG1 = 'E',            /* magic number byte 1 */
	
	EI_MAG2 = 2,              /* file identification byte 2 index */
	ELFMAG2 = 'L',            /* magic number byte 2 */
	
	EI_MAG3 = 3,              /* file identification byte 3 index */
	ELFMAG3 = 'F',            /* magic number byte 3 */
};

/**
 * Conglomeration of the identification bytes, for easy testing as a word
 */
const char *ELFMAG = "\177ELF";

enum {
	SELFMAG       = 4,

	EI_CLASS      = 4,   /* file class byte index */
	ELFCLASSNONE  = 0,   /* invalid class         */
	ELFCLASS32    = 1,   /* 32-bit objects        */
	ELFCLASS64    = 2,   /* 64-bit objects        */
	ELFCLASSNUM   = 3,

	EI_DATA       = 5,   /* data encoding byte index      */
	ELFDATANONE   = 0,   /* invalid data encoding         */
	ELFDATA2LSB   = 1,   /* 2's complement, little endian */
	ELFDATA2MSB   = 2,   /* 2's complement, big endian    */
	ELFDATANUM    = 3,

	EI_ABIVERSION = 8,   /* ABI version */

	EI_PAD        = 9,   /* byte index of padding bytes */
};

/**
 * Legal values for e_type (object file type)
 */
enum {
	ET_NONE = 0,   /* no file type    */
	ET_EXEC = 2,   /* executable file */
	ET_DYN  = 3,   /* shared object file */
};

/**
 * Legal values for e_machine (architecture)
 */
enum {
	EM_NONE = 0,   /* no machine  */
	EM_386  = 3,   /* intel 80386 */
};

/**
 * Legal values for e_version (version)
 */
enum {
	EV_NONE    = 0,   /* invalid ELF version */
	EV_CURRENT = 1,   /* current version     */
	EV_NUM     = 2,
};

/**
 * Program segment header
 */
typedef struct
{
	Elf32_Word    p_type;     /* segment type             */
	Elf32_Off     p_offset;   /* segment file offset      */
	Elf32_Addr    p_vaddr;    /* segment virtual address  */
	Elf32_Addr    p_paddr;    /* segment physical address */
	Elf32_Word    p_filesz;   /* segment size in file     */
	Elf32_Word    p_memsz;    /* segment size in memory   */
	Elf32_Word    p_flags;    /* segment flags            */
	Elf32_Word    p_align;    /* segment alignment        */
} Elf32_Phdr;

typedef struct
{
	Elf64_Word    p_type;     /* segment type             */
	Elf64_Word    p_flags;    /* segment flags            */
	Elf64_Off     p_offset;   /* segment file offset      */
	Elf64_Addr    p_vaddr;    /* segment virtual address  */
	Elf64_Addr    p_paddr;    /* segment physical address */
	Elf64_Xword   p_filesz;   /* segment size in file     */
	Elf64_Xword   p_memsz;    /* segment size in memory   */
	Elf64_Xword   p_align;    /* segment alignment        */
} Elf64_Phdr;

/**
 * Legal values for p_type (segment type)
 */
enum {
	PT_NULL         = 0,            /* program header table entry unused */
	PT_LOAD         = 1,            /* loadable program segment          */
	PT_DYNAMIC      = 2,            /* dynamic linking information       */
	PT_INTERP       = 3,            /* program interpreter               */
	PT_NOTE         = 4,            /* auxiliary information             */
	PT_SHLIB        = 5,            /* reserved                          */
	PT_PHDR         = 6,            /* entry for header table itself     */
	PT_TLS          = 7,            /* thread-local storage segment      */
	PT_NUM          = 8,            /* number of defined types           */
	PT_LOOS         = 0x60000000,   /* start of OS-specific              */
	PT_GNU_EH_FRAME = 0x6474e550,   /* gcc .eh_frame_hdr segment         */
	PT_GNU_STACK    = 0x6474e551,   /* indicates stack executability     */
	PT_GNU_RELRO    = 0x6474e552,   /* read-only after relocation        */
	PT_LOPROC       = 0x70000000,   /* first processor-specific type     */
	PT_HIPROC       = 0x7fffffff,   /* last processor-specific type      */
};

/**
 * Legal values for p_flags (segment flags)
 */
enum {
	PF_X = (1 << 0),   /* segment is executable */
	PF_W = (1 << 1),   /* segment is writable   */
	PF_R = (1 << 2),   /* segment is readable   */
};

/**
 * Define bit-width independent types
 */

#ifdef _LP64
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
#define ELFCLASS ELFCLASS64
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
#define ELFCLASS ELFCLASS32
#endif /* _LP64 */

} /* extern "C" */

#endif /* _INCLUDE__BASE__INTERNAL__ELF_FORMAT_H_ */
