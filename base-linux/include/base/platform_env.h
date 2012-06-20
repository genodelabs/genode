/*
 * \brief  Linux-specific environment
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PLATFORM_ENV_H_
#define _INCLUDE__BASE__PLATFORM_ENV_H_

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <base/env.h>
#include <base/printf.h>
#include <util/misc_math.h>
#include <base/local_interface.h>
#include <base/heap.h>
#include <parent/client.h>
#include <ram_session/client.h>
#include <cpu_session/client.h>

namespace Genode {

	class Platform_env : public Env
	{
		private:

			/**************************
			 ** Local region manager **
			 **************************/

			class Region
			{
				private:

					addr_t               _start;
					off_t                _offset;
					Dataspace_capability _ds;
					size_t               _size;

					/**
					 * Return offset of first byte after the region
					 */
					addr_t _end() const { return _start + _size; }

				public:

					Region() : _start(0), _offset(0), _size(0) { }

					Region(addr_t start, off_t offset, Dataspace_capability ds, size_t size)
					: _start(start), _offset(offset), _ds(ds), _size(size) { }

					bool                 used()      const { return _size > 0; }
					addr_t               start()     const { return _start; }
					off_t                offset()    const { return _offset; }
					size_t               size()      const { return _size; }
					Dataspace_capability dataspace() const { return _ds; }

					bool intersects(Region const &r) const
					{
						return (r.start() < _end()) && (_start < r._end());
					}
			};


			/**
			 * Meta data about dataspaces attached to an RM session
			 */
			class Region_map
			{
				public:

					enum { MAX_REGIONS = 4096 };

				private:

					Region _map[MAX_REGIONS];

					bool _id_valid(int id) const {
						return (id >= 0 && id < MAX_REGIONS); }

				public:

					/**
					 * Add region to region map
					 *
					 * \return region ID, or
					 *         -1 if out of metadata, or
					 *         -2 if region conflicts existing region
					 */
					int add_region(Region const &region)
					{
						/*
						 * Check for region conflicts
						 */
						for (int i = 0; i < MAX_REGIONS; i++) {
							if (_map[i].intersects(region))
								return -2;
						}

						/*
						 * Allocate new region metadata
						 */
						int i;
						for (i = 0; i < MAX_REGIONS; i++)
							if (!_map[i].used()) break;

						if (i == MAX_REGIONS) {
							PERR("maximum number of %d regions reached",
							     MAX_REGIONS);
							return -1;
						}

						_map[i] = region;
						return i;
					}

					Region region(int id) const
					{
						return _id_valid(id) ? _map[id] : Region();
					}

					Region lookup(addr_t start)
					{
						for (int i = 0; i < MAX_REGIONS; i++)
							if (_map[i].start() == start)
								return _map[i];
						return Region();
					}

					void remove_region(addr_t start)
					{
						for (int i = 0; i < MAX_REGIONS; i++)
							if (_map[i].start() == start)
								_map[i] = Region();
					}
			};


			/*
			 * On Linux, we use a local region manager session
			 * that attaches dataspaces via mmap to the local
			 * address space.
			 */
			class Rm_session_mmap : public Local_interface,
			                        public Rm_session,
			                        public Dataspace
			{
				private:

					Lock         _lock;    /* protect '_rmap' */
					Region_map   _rmap;
					bool   const _sub_rm;  /* false if RM session is root */
					size_t const _size;

					/**
					 * Base offset of the RM session
					 *
					 * For a normal RM session (the one that comes with the
					 * 'env()', this value is zero. If the RM session is
					 * used as nested dataspace, '_base' contains the address
					 * where the managed dataspace is attached in the root RM
					 * session.
					 *
					 * Note that a managed dataspace cannot be attached more
					 * than once. Furthermore, managed dataspace cannot be
					 * attached to another managed dataspace. The nested
					 * dataspace emulation is solely implemented to support
					 * the common use case of managed dataspaces as mechanism
					 * to reserve parts of the local address space from being
					 * populated by the 'env()->rm_session()'. (i.e., for the
					 * context area, or for the placement of consecutive
					 * shared-library segments)
					 */
					addr_t _base;

					bool _is_attached() const { return _base > 0; }

					void _add_to_rmap(Region const &);

				public:

					Rm_session_mmap(bool sub_rm, size_t size = ~0)
					: _sub_rm(sub_rm), _size(size), _base(0) { }

					~Rm_session_mmap()
					{
						/* detach sub RM session when destructed */
						if (_sub_rm && _is_attached())
							env()->rm_session()->detach((void *)_base);
					}


					/**************************************
					 ** Region manager session interface **
					 **************************************/

					Local_addr attach(Dataspace_capability ds, size_t size,
					                  off_t, bool, Local_addr,
					                  bool executable);

					void detach(Local_addr local_addr);

					Pager_capability add_client(Thread_capability thread) {
						return Pager_capability(); }

					void fault_handler(Signal_context_capability handler) { }

					State state() { return State(); }


					/*************************
					 ** Dataspace interface **
					 *************************/

					size_t size() { return _size; }

					addr_t phys_addr() { return 0; }

					bool writable() { return true; }

					/**
					 * Return pseudo dataspace capability of the RM session
					 *
					 * The capability returned by this function is only usable
					 * as argument to 'Rm_session_mmap::attach'. It is not a
					 * real capability.
					 */
					Dataspace_capability dataspace()
					{
						return Local_interface::capability(this);
					}
			};


			class Expanding_ram_session_client : public Ram_session_client
			{
				Ram_session_capability _cap;

				public:

					Expanding_ram_session_client(Ram_session_capability cap)
					: Ram_session_client(cap), _cap(cap) { }

					Ram_dataspace_capability alloc(size_t size, bool cached) {
						bool try_again;
						do {
							try_again = false;
							try {
								return Ram_session_client::alloc(size, cached);

							} catch (Ram_session::Out_of_metadata) {

								/* give up if the error occurred a second time */
								if (try_again)
									break;

								PINF("upgrade quota donation for Env::RAM session");
								env()->parent()->upgrade(_cap, "ram_quota=8K");
								try_again = true;
							}
						} while (try_again);

						return Ram_dataspace_capability();
					}
			};


			/**
			 * Local interceptor of parent requests
			 *
			 * On Linux, we need to intercept calls to the parent interface to
			 * implement the RM service locally. This particular service is
			 * used for creating managed dataspaces, which allow the
			 * reservation of parts of the local address space from being
			 * automatically managed by the 'env()->rm_session()'.
			 *
			 * All requests that do not refer to the RM service are passed
			 * through the real parent interface.
			 */
			class Local_parent : public Parent_client
			{
				public:

					/**********************
					 ** Parent interface **
					 **********************/

					Session_capability session(Service_name const &,
					                           Session_args const &);
					void close(Session_capability);

					/**
					 * Constructor
					 *
					 * \param parent_cap  real parent capability used to
					 *                    promote requests to non-local
					 *                    services
					 */
					Local_parent(Parent_capability parent_cap);
			};


			/*************************************
			 ** Linux-specific helper functions **
			 *************************************/

			/**
			 * Read Unix environment variable as long value
			 */
			static unsigned long _get_env_ulong(const char *key);


			Parent_capability _parent_cap()
			{
				long tid        = _get_env_ulong("parent_tid");
				long local_name = _get_env_ulong("parent_local_name");

				/* produce typed capability manually */
				return reinterpret_cap_cast<Parent>(Native_capability(tid, local_name));
			}


			/*******************************
			 ** Platform-specific members **
			 *******************************/

			Local_parent                  _parent;
			Ram_session_capability        _ram_session_cap;
			Expanding_ram_session_client  _ram_session_client;
			Cpu_session_client            _cpu_session_client;
			Rm_session_mmap               _rm_session_mmap;
			Heap                          _heap;

		public:

			/**
			 * Standard constructor
			 */
			Platform_env()
			:
				_parent(_parent_cap()),
				_ram_session_cap(static_cap_cast<Ram_session>(parent()->session("Env::ram_session", ""))),
				_ram_session_client(_ram_session_cap),
				_cpu_session_client(static_cap_cast<Cpu_session>(parent()->session("Env::cpu_session", ""))),
				_rm_session_mmap(false),
				_heap(&_ram_session_client, &_rm_session_mmap)
			{ }

			/**
			 * Destructor
			 */
			~Platform_env() { parent()->exit(0); }

			/**
			 * Reload parent capability and reinitialize environment resources
			 */
			void reload_parent_cap(Capability<Parent>::Dst, long)
			{
				/* not supported on Linux */
			}


			/*******************
			 ** Env interface **
			 *******************/

			Parent                 *parent()          { return &_parent; }
			Ram_session            *ram_session()     { return &_ram_session_client; }
			Ram_session_capability  ram_session_cap() { return  _ram_session_cap; }
			Rm_session             *rm_session()      { return &_rm_session_mmap; }
			Heap                   *heap()            { return &_heap; }
			Cpu_session            *cpu_session()     { return &_cpu_session_client; }
			Pd_session             *pd_session()      { return 0; }
	};
}

#endif /* _INCLUDE__BASE__PLATFORM_ENV_H_ */
