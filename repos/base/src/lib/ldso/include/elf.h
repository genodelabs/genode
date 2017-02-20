/**
 * \brief  ELF binary definitions
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2014-05-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ELF_H_
#define _INCLUDE__ELF_H_


#include <base/stdint.h>

namespace Linker {

	/* standard ELF types.  */

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
	 * Fields in the e_ident array of ELF file header The EI_* macros are indices
	 * into the array.  The macros under each EI_* macro are the values the byte
	 * may have.
	 */
	enum {
		EI_NIDENT = 16,           /* size of e_ident array in ELF header */

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
	extern const char *ELFMAG;

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
		PT_ARM_EXIDX    = 0x70000001,   /* location of exception tables      */
		PT_HIPROC       = 0x7fffffff,   /* last processor-specific type      */
	};

	/**
	 * Legal values for p_flags (segment flags)
	 */
	enum {
		PF_X    = (1 << 0),   /* segment is executable */
		PF_W    = (1 << 1),   /* segment is writable   */
		PF_R    = (1 << 2),   /* segment is readable   */
		PF_MASK = 0x7,
	};

	/**
	 * Tag value for Elf::Dyn
	 */
	enum D_tag
	{
		DT_NULL     = 0,
		DT_NEEDED   = 1,   /* dependend libraries */
		DT_PLTRELSZ = 2,   /* size of PLT relocations */
		DT_PLTGOT   = 3,   /* processor dependent address */
		DT_HASH     = 4,   /* address of symbol hash table   */
		DT_STRTAB   = 5,   /* string table */
		DT_SYMTAB   = 6,   /* address of symbol table */
		DT_RELA     = 7,   /* ELF relocation with addend     */
		DT_RELASZ   = 8,   /* total size of RELA reolcations */
		DT_STRSZ    = 10,  /* size of string table */
		DT_INIT     = 12,  /* ctors */
		DT_REL      = 17,  /* address of Elf::Rel relocations */
		DT_RELSZ    = 18,  /* sizof Elf::Rel relocation       */
		DT_PLTREL   = 20,  /* PLT relcation */
		DT_DEBUG    = 21,  /* debug structure location */
		DT_JMPREL   = 23,  /* address of PLT relocation */
	};


	/**
	 * Symbol table
	 */
	enum Symbol_table {
		/* Termination symbol for hash chains */
		STN_UNDEF = 0,

		/* Bindings */
		STB_LOCAL = 0,  /* local symbol */
		STB_WEAK  = 2,  /* weak symbol */

		/* Types */
		STT_NOTYPE = 0, /* type unspecified */
		STT_OBJECT = 1, /* data             */
		STT_FUNC   = 2, /* function         */

		/* Section table index */
		SHN_UNDEF  = 0,      /* undefined */
		SHN_COMMON = 0xfff2, /* common data */
};


/********************************
 ** 32-Bit non-POD definitions **
 ********************************/

	namespace Elf32 {

		typedef Elf32_Addr Addr;
		typedef Elf32_Word Hashelt;
		typedef Elf32_Word Size;
		typedef Elf32_Half Half;

		/**
		 * The ELF file header
		 */
		struct Ehdr
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
		};


		/**
		 * Program segment header
		 */
		struct Phdr
		{
			Elf32_Word    p_type;     /* segment type             */
			Elf32_Off     p_offset;   /* segment file offset      */
			Elf32_Addr    p_vaddr;    /* segment virtual address  */
			Elf32_Addr    p_paddr;    /* segment physical address */
			Elf32_Word    p_filesz;   /* segment size in file     */
			Elf32_Word    p_memsz;    /* segment size in memory   */
			Elf32_Word    p_flags;    /* segment flags            */
			Elf32_Word    p_align;    /* segment alignment        */
		};

		/**
		 * Dynamic structure (section .dynamic)
		 */
		struct Dyn
		{
			Elf32_Sword  tag; /* entry type    */
			union {
				Elf32_Word val; /* integer value */
				Elf32_Addr ptr; /* address value */
			} un;
		};

		/**
		 * Relocation
		 */
		struct Rel
		{
			Elf32_Addr  offset; /* location to be relocated         */
			Elf32_Word  info;   /* relocation type and symbol index */

			/**
			 * Relocation type
			 */
			int type()     const { return info & 0xff; }

			/**
			 * Symbol table index
			 */
			unsigned sym() const { return info >> 8; }
		};

		/**
		 * Relocations that need an addend field
		 */
		struct Rela
		{
			Elf32_Addr  r_offset; /* location to be relocated          */
			Elf32_Word  r_info;   /* relocation type and symbol index  */
			Elf32_Sword r_addend; /* addend                            */
		};

		/**
		 * Symbol table entry
		 */
		struct Sym
		{
			Elf32_Word    st_name;  /* string table index of name   */
			Elf32_Addr    st_value; /* symbol value                 */
			Elf32_Word    st_size;  /* size of associated object    */
			unsigned char st_info;  /* type and binding information */
			unsigned char st_other; /* reserved (not used)          */
			Elf32_Half    st_shndx; /* section index of symbol      */

			/**
			 * Binding information
			 */
			unsigned char bind() const { return st_info >> 4; }

			/**
			 * Type information
			 */
			unsigned char type() const { return st_info & 0xf; }

			/**
			 * Check for weak symbol
			 */
			bool weak() const { return bind() == STB_WEAK; }
		};
	}


/********************************
 ** 64-Bit non-POD definitions **
 ********************************/

	namespace Elf64 {

		typedef Elf64_Addr  Addr;
		typedef Elf64_Word  Hashelt;
		typedef Elf64_Xword Size;
		typedef Elf64_Half  Half;

		/**
		 * ELF header
		 */
		struct Ehdr
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
		};

		/**
		 * Program header
		 */
		struct Phdr
		{
			Elf64_Word    p_type;     /* segment type             */
			Elf64_Word    p_flags;    /* segment flags            */
			Elf64_Off     p_offset;   /* segment file offset      */
			Elf64_Addr    p_vaddr;    /* segment virtual address  */
			Elf64_Addr    p_paddr;    /* segment physical address */
			Elf64_Xword   p_filesz;   /* segment size in file     */
			Elf64_Xword   p_memsz;    /* segment size in memory   */
			Elf64_Xword   p_align;    /* segment alignment        */
		};

		/**
		 * Dynamic structure (section .dynamic)
		 */
		struct Dyn
		{
			Elf64_Sxword tag;  /* entry type.    */
			union
			{
				Elf64_Xword val; /* integer value. */
				Elf64_Addr  ptr; /* address value. */
			} un;
		};

		/**
		 * Relocation
		 */
		struct Rel
		{
			Elf64_Addr  r_offset; /* location to be relocated. */
			Elf64_Xword r_info;   /* relocation type and symbol index. */
		};

		/**
		 * Relocations that need an addend field
		 */
		struct Rela
		{
			Elf64_Addr   offset;  /* location to be relocated         */
			Elf64_Xword  info;    /* relocation type and symbol index */
			Elf64_Sxword addend;  /* addend                           */

			/**
			 * Relocation type
			 */
			int type()     const { return info & 0xffffffffL; }

			/**
			 * Symbol table index
			 */
			unsigned sym() const { return (info >> 16) >> 16; }
		};

		/**
		 * Symbol table entry
		 */
		struct Sym
		{
			Elf64_Word    st_name;  /* string table index of name   */
			unsigned char st_info;  /* type and binding information */
			unsigned char st_other; /* reserved (not used)          */
			Elf64_Half    st_shndx; /* section index of symbol      */
			Elf64_Addr    st_value; /* symbol value                 */
			Elf64_Xword   st_size;  /* size of associated object    */

			/**
			 * Binding information
			 */
			unsigned char bind() const { return st_info >> 4; }

			/**
			 * Type information
			 */
			unsigned char type() const { return st_info & 0xf; }

			/**
			 * Check for weak symbol
			 */
			bool weak() const { return bind() == STB_WEAK; }
		};
	} /* namespace Elf64 */
} /* namespace Linker" */

/**
 * Define bit-width independent types
 */
#ifdef _LP64
namespace Elf = Linker::Elf64;
#define ELFCLASS ELFCLASS64
#define EFMT "%llx"
#else
namespace Elf = Linker::Elf32;
#define ELFCLASS ELFCLASS32
#define EFMT "%x"
#endif /* _LP64 */

#endif /* _INCLUDE__ELF_H_ */
