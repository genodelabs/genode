/*
 * \brief  ELF binary utility
 * \author Christian Helmuth
 * \date   2006-05-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ELF_H_
#define _INCLUDE__BASE__ELF_H_

#include <base/stdint.h>

namespace Genode {

	class Elf_segment;
	class Elf_binary;
}


class Genode::Elf_binary
{
	public:

		/**
		 * Default constructor creates invalid object
		 */
		Elf_binary() { }

		/**
		 * Constructor
		 *
		 * The object is only useful if valid() returns true.
		 */
		explicit Elf_binary(addr_t start);

		/* special types */

		struct Flags {
			unsigned r:1;
			unsigned w:1;
			unsigned x:1;
			unsigned skip:1;
		};

		/**
		 * Read information about program segments
		 *
		 * \return  properties of the specified program segment
		 */
		Elf_segment get_segment(unsigned num);

		/**
		 * Check validity
		 */
		bool valid() { return _valid; }

		/**
		 * Check for dynamic elf
		 */
		bool dynamically_linked() { return (_dynamic && _interp); }


		/************************
		 ** Accessor functions **
		 ************************/

		addr_t entry() { return valid() ? _entry : 0; }

	private:

		/* validity indicator indicates if the loaded ELF is valid and supported */
		bool _valid { false };

		/* dynamically linked */
		bool _dynamic { false };

		/* dynamic linker name matches 'genode' */
		bool _interp { false };

		/* ELF start pointer in memory */
		addr_t _start { 0 };

		/* ELF entry point */
		addr_t _entry { 0 };

		/* program segments */
		addr_t   _ph_table  { 0 };
		size_t   _phentsize { 0 };
		unsigned _phnum     { 0 };


		/************
		 ** Helper **
		 ************/

		/**
		 * Check ELF header compatibility
		 */
		int _ehdr_check_compat();

		/**
		 * Check program header compatibility
		 */
		int _ph_table_check_compat();

		/**
		 * Check for dynamic program segments
		 */
		bool _dynamic_check_compat(unsigned type);
};


class Genode::Elf_segment
{
	public:

		/**
		 * Standard constructor creates invalid object
		 */
		Elf_segment() { }

		Elf_segment(const Elf_binary *elf, void *start, size_t file_offset,
		            size_t file_size, size_t mem_size, Elf_binary::Flags flags)
		: _elf(elf), _start((unsigned char *)start), _file_offset(file_offset),
		  _file_size(file_size), _mem_size(mem_size), _flags(flags)
		{
			_valid = elf ? true : false;
		}

		Elf_binary const *  elf() { return _elf; }
		void            * start() { return (void *)_start; }
		size_t      file_offset() { return _file_offset; }
		size_t        file_size() { return _file_size; }
		size_t         mem_size() { return _mem_size; }
		Elf_binary::Flags flags() { return _flags; }

		/**
		 * Check validity
		 */
		bool valid() { return _valid; }

	private:

		Elf_binary const *_elf         { nullptr };
		bool              _valid       { false };
		unsigned char    *_start       { nullptr };
		size_t            _file_offset { 0 };
		size_t            _file_size   { 0 };
		size_t            _mem_size    { 0 };
		Elf_binary::Flags _flags       { };
};

#endif /* _INCLUDE__BASE__ELF_H_ */
