/*
 * \brief  Dynamic linker interface
 * \author Sebastian Sumpf
 * \date   2014-10-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SHARED_OBJECT_H_
#define _INCLUDE__BASE__SHARED_OBJECT_H_

#include <base/exception.h>
#include <base/stdint.h>

namespace Genode {

	class  Shared_object;
	struct Address_info;
};


class Genode::Shared_object
{
	private:

		void *_handle = nullptr;

		void *_lookup(const char *symbol) const;

	public:

		class Invalid_file   : public Genode::Exception { };
		class Invalid_symbol : public Genode::Exception { };

		enum open_flags {
			NOW    = 0x1,
			LAZY   = 0x2,
			KEEP   = 0x4, /* do not unload on destruction */
		};

		/**
		 * Load shared object and dependencies
		 *
		 * \param file   Shared object to load
		 * \param flags  LAZY for lazy function binding, NOW for immediate binding
		 *
		 * \throw Invalid_file
		 */
		Shared_object(char const *file = nullptr, unsigned flags = LAZY);

		/**
		 * Close and unload shared object
		 */
		~Shared_object();

		/**
		 * Lookup a symbol in shared object and it's dependencies
		 *
		 * \param symbol  Symbol name
		 *
		 * \throw Invalid_symbol
		 *
		 * \return Symbol address
		 */
		template<typename T = void *> T lookup(const char *symbol) const
		{
			return reinterpret_cast<T>(_lookup(symbol));
		}

		/**
		 * Link information
		 */
		struct Link_map
		{
			Genode::addr_t  addr;    /* load address */
			char const     *path;    /* object path  */
			void const     *dynamic; /* pointer to DYNAMIC section */
			Link_map       *next;
			Link_map       *prev;
		};

		/**
		 * Return link map of shared object
		 */
		Link_map const * link_map() const;
};


struct Genode::Address_info
{
	char const    *path; /* path of shared object */
	Genode::addr_t base; /* base of shared object */
	char const    *name; /* name of symbol        */
	Genode::addr_t addr; /* address of symbol     */

	class Invalid_address : public Genode::Exception { };

	Address_info(Genode::addr_t addr);
};

#endif /* _INCLUDE__BASE__SHARED_OBJECT_H_ */
