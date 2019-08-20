/*
 * \brief  Dynamic linker interface
 * \author Sebastian Sumpf
 * \date   2014-10-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SHARED_OBJECT_H_
#define _INCLUDE__BASE__SHARED_OBJECT_H_

#include <base/env.h>
#include <base/allocator.h>
#include <base/exception.h>
#include <base/stdint.h>
#include <rom_session/rom_session.h>

namespace Genode {

	class  Shared_object;
	struct Address_info;
	struct Dynamic_linker;
};


class Genode::Shared_object
{
	private:

		void *_handle = nullptr;
		void *_lookup(const char *symbol) const;

		Allocator &_md_alloc;

		/*
		 * Noncopyable
		 */
		Shared_object(Shared_object const &);
		Shared_object &operator = (Shared_object const &);

	public:

		class Invalid_rom_module : public Genode::Exception { };
		class Invalid_symbol     : public Genode::Exception { };

		enum Keep { DONT_KEEP, KEEP };
		enum Bind { BIND_LAZY, BIND_NOW };

		/**
		 * Load shared object and dependencies
		 *
		 * \param env       Genode environment, needed to obtain the ROM module
		 *                  for the shared object and to populate the linker
		 *                  area of the component's address space
		 * \param md_alloc  backing store for the linker's dynamically allocated
		 *                  meta data
		 * \param name      ROM module name of shared object to load
		 * \param bind      bind functions immediately (BIND_NOW) or on
		 *                  demand (BIND_LAZY)
		 * \param keep      unload ELF object if no longer needed (DONT_KEEP),
		 *                  or keep ELF object loaded at all times (KEEP)
		 *
		 * \throw Invalid_rom_module
		 */
		Shared_object(Env &, Allocator &md_alloc, char const *name,
		              Bind bind, Keep keep);

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
			char     const *path;    /* object path  */
			void     const *dynamic; /* pointer to DYNAMIC section */
			Link_map const *next;
			Link_map const *prev;
		};

		/**
		 * Return link map of shared object
		 */
		Link_map const &link_map() const;
};


struct Genode::Address_info
{
	char const    *path { nullptr }; /* path of shared object */
	Genode::addr_t base { 0 };       /* base of shared object */
	char const    *name { nullptr }; /* name of symbol        */
	Genode::addr_t addr { 0 };       /* address of symbol     */

	class Invalid_address : public Genode::Exception { };

	Address_info(Genode::addr_t addr);
};


class Genode::Dynamic_linker
{
	public:

		struct Object_info
		{
			/* name of shared library, or "binary" for the main program */
			typedef String<64> Name;
			Name name;

			Rom_dataspace_capability ds_cap;

			/* pointer to the start of the read/writeable segment */
			void *rw_start;

			/* size of the read-writeable segment in bytes */
			size_t rw_size;
		};

	private:

		struct For_each_fn : Interface
		{
			virtual void supply_object_info(Object_info const &) const = 0;
		};

		static void _for_each_loaded_object(Env &, For_each_fn const &);

		static void *_respawn(Env &, char const *, char const *);

	public:

		/**
		 * Call 'fn' for each loaded object with 'Object_info' as argument
		 */
		template <typename FN>
		static inline void for_each_loaded_object(Env &env, FN const &fn)
		{
			struct For_each_fn_impl : For_each_fn
			{
				FN const &fn;

				void supply_object_info(Object_info const &info) const { fn(info); }

				For_each_fn_impl(FN const &fn) : fn(fn) { }

			} wrapped_fn { fn };

			_for_each_loaded_object(env, wrapped_fn);
		}

		/**
		 * Prevent loaded shared object 'name' to be unloaded
		 */
		static void keep(Env &, char const *name);

		typedef Shared_object::Invalid_rom_module Invalid_rom_module;
		typedef Shared_object::Invalid_symbol     Invalid_symbol;

		/**
		 * Replace executable binary
		 *
		 * \param binary_name   ROM module name of new executable binary
		 * \param start_symbol  symbol name of the binary's entry point
		 *
		 * \return  pointer to entry point of the new executable
		 *
		 * \throw Invalid_rom_module
		 * \throw Invalid_symbol
		 */
		template<typename T = void *>
		static T respawn(Env &env, char const *binary_name, char const *entrypoint_name)
		{
			return reinterpret_cast<T>(_respawn(env, binary_name, entrypoint_name));
		}
};

#endif /* _INCLUDE__BASE__SHARED_OBJECT_H_ */
