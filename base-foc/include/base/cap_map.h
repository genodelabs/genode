/*
 * \brief  Mapping of Genode's capability names to kernel capabilities.
 * \author Stefan Kalkowski
 * \date   2012-02-16
 *
 * Although kernels like Fiasco.OC and NOVA provide capability mechanisms
 * to us, which should prevent the usage of global names, there is no
 * efficient way to retrieve a capability a process owns, when it gets the
 * same capability delivered again via IPC from another thread. But in some
 * use-cases in Genode this is essential (e.g. parent getting a close-session
 * request from a child). Moreover, we waste a lot of slots in the
 * capability-space of the process for one and the same kernel-object.
 * That's why we introduce a map of Genode's global capability names to the
 * process-local addresses in the capability-space.
 *
 * TODO: should be moved to the generic part of the framework, and used by
 *       NOVA too.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAP_MAP_H_
#define _INCLUDE__BASE__CAP_MAP_H_

/* Genode includes */
#include <base/exception.h>
#include <base/stdint.h>
#include <base/lock_guard.h>
#include <util/avl_tree.h>
#include <util/noncopyable.h>
#include <util/string.h>

namespace Genode
{
	/**
	 * A Cap_index represents a single mapping of the global capability id
	 * to the address in the local capability space.
	 *
	 * The address of the Cap_index determines the location in the
	 * (platform-specific) capability space of the process. Therefore it
	 * shouldn't be copied around, but only referenced by
	 * e.g. Native_capability.
	 */
	class Cap_index : public Avl_node<Cap_index>,
	                         Noncopyable
	{
		private:

			enum { INVALID_ID = -1, UNUSED = 0 };

			uint8_t  _ref_cnt; /* reference counter    */
			uint16_t _id;      /* global capability id */

		public:

			Cap_index() : _ref_cnt(0), _id(INVALID_ID) { }

			bool     valid() const   { return _id != INVALID_ID; }
			bool     used()  const   { return _id != UNUSED;     }
			uint16_t id()    const   { return _id;               }
			void     id(uint16_t id) { _id = id;                 }
			uint8_t  inc();
			uint8_t  dec();
			addr_t   kcap();

			void* operator new    (size_t size, Cap_index* idx) { return idx; }
			void  operator delete (void* idx) { memset(idx, 0, sizeof(Cap_index)); }


			/************************
			 ** Avl node interface **
			 ************************/

			bool higher(Cap_index *n);
			Cap_index *find_by_id(uint16_t id);
	};


	/**
	 * Allocator for Cap_index objects.
	 *
	 * This is just an interface, as the real allocator has to be
	 * implemented platform-specific.
	 */
	class Cap_index_allocator: Noncopyable
	{
		public:

			class Index_out_of_bounds : public Exception { };
			class Region_conflict     : public Exception { };

			virtual ~Cap_index_allocator() {}

			/**
			 * Allocate a range of Cap_index objects
			 *
			 * \param  cnt number of objects to allocate
			 * \return     pointer to first allocated object, or zero if
			 *             out of entries
			 */
			virtual Cap_index* alloc(size_t cnt) = 0;

			/**
			 * Allocate a range of Cap_index objects at a specific
			 * point in the capability space
			 *
			 * \param  kcap                address in capability space
			 * \param  cnt                 number of objects to allocate
			 * \throw  Index_out_of_bounds if address is out of scope
			 * \throw  Region_conflict     if capability space entry is used
             * \return                     pointer to first allocated object,
			 *                             or zero if out of entries
			 */
			virtual Cap_index* alloc(addr_t kcap, size_t cnt) = 0;

			/**
			 * Free a range of Cap_index objects
			 *
			 * \param idx                 pointer to first object in range
			 * \param cnt                 number of objects to free
			 * \throw Index_out_of_bounds if address is out of scope
			 */
			virtual void free(Cap_index *idx, size_t cnt) = 0;

			/**
			 * Get the Cap_index object's address in capability space
			 *
			 * \param idx pointer to the Cap_index object in question
			 */
			virtual addr_t idx_to_kcap(Cap_index *idx) = 0;

			/**
			 * Get the Cap_index object of a specific location
			 * in the capability space
			 *
			 * \param kcap the address in the capability space
			 */
			virtual Cap_index* kcap_to_idx(addr_t kcap) = 0;
	};


	/**
	 * Get the global Cap_index_allocator of the process.
	 */
	Cap_index_allocator *cap_idx_alloc();


	/**
	 * Low-level spin-lock to protect Cap_index_allocator and the Cap_map
	 *
	 * We cannot use a normal Genode lock because this lock is used by code
	 * executed prior the initialization of Genode
	 */
	class Spin_lock
	{
		private:

			volatile int _spinlock;

		public:

			/**
			 * Constructor
			 */
			Spin_lock();

			void lock();
			void unlock();

			/**
			 * Lock guard
			 */
			typedef Genode::Lock_guard<Spin_lock> Guard;
	};


	class Native_capability;


	/**
	 * The Capability_map is an AVL-tree of Cap_index objects that can be
	 * found via the global capability id
	 *
	 * It is used to re-find capabilities whenever a capability gets
	 * transfered to a process, so that we can re-use an existing one
	 * to save entries in the capability space, and prevent leaks of
	 * them.
	 */
	class Capability_map : Noncopyable
	{
		private:

			Avl_tree<Cap_index> _tree;
			Spin_lock           _lock;

		public:

			/**
			 * Find an existing Cap_index via a capability id
			 *
			 * \param  id the global capability id
			 * \return    pointer of Cap_index when found, otherwise zero
			 */
			Cap_index* find(int id);

			/**
			 * Create and insert a new Cap_index with a specific capability id
			 *
			 * Allocation of the Cap_index is done via the global
			 * Cap_index_allocator, which might throw exceptions that aren't
			 * caught by this method
			 *
			 * \param  id the global capability id
			 * \return    pointer to the new Cap_index object, or zero
			 *            when allocation failed
			 */
			Cap_index* insert(int id);


			/**
			 * Create and insert a new Cap_index with a specific capability id,
			 * and location in capability space
			 *
			 * Allocation of the Cap_index is done via the global
			 * Cap_index_allocator, which might throw exceptions that aren't
			 * caught by this method
			 *
			 * \param  id   the global capability id
			 * \param  kcap address in capability space
			 * \return      pointer to the new Cap_index object, or zero
			 *              when allocation failed
			 */
			Cap_index* insert(int id, addr_t kcap);

			/**
			 * Create and insert a new Cap_index with a specific capability id
			 * and map from given kcap to newly allocated one
			 *
			 * Allocation of the Cap_index is done via the global
			 * Cap_index_allocator, which might throw exceptions that aren't
			 * caught by this method
			 *
			 * \param  id the global capability id
			 * \return    pointer to the new Cap_index object, or zero
			 *            when allocation failed
			 */
			Cap_index* insert_map(int id, addr_t kcap);

			/**
			 * Remove a Cap_index object
			 *
			 * \param i pointer to Cap_index object to remove
			 */
			void remove(Cap_index* i);
	};


	/**
	 * Get the global Capability_map of the process.
	 */
	Capability_map *cap_map();
}

#endif /* _INCLUDE__BASE__CAP_MAP_H_ */
