/*
 * \brief  Capability-space management for base-linux
 * \author Norman Feske
 * \date   2016-06-15
 *
 * On Linux, a capability is represented by a socket descriptor and an RPC
 * object key. The thread ID respectively socket descriptor refer to the
 * recipient of an RPC call (RPC destination).
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_

/* base includes */
#include <util/avl_tree.h>
#include <util/bit_allocator.h>
#include <base/mutex.h>
#include <base/log.h>
#include <util/construct_at.h>

/* base-internal includes */
#include <base/internal/capability_space.h>
#include <base/internal/rpc_destination.h>
#include <linux_syscalls.h>

namespace Genode { template <unsigned, typename> class Capability_space_tpl; }


/**
 * Platform-specific supplement to the generic 'Capability_space' interface
 */
namespace Genode { namespace Capability_space {

	/**
	 * Information needed to transfer capability via the kernel's IPC mechanism
	 */
	struct Ipc_cap_data
	{
		Rpc_destination dst;
		Rpc_obj_key rpc_obj_key;

		Ipc_cap_data(Rpc_destination dst, Rpc_obj_key rpc_obj_key)
		: dst(dst), rpc_obj_key(rpc_obj_key) { }

		void print(Output &out) const { Genode::print(out, dst, ",", rpc_obj_key); }
	};

	/**
	 * Retrieve IPC data for given capability
	 */
	Ipc_cap_data ipc_cap_data(Native_capability const &cap);

	Native_capability lookup(Rpc_obj_key);

	Native_capability import(Rpc_destination, Rpc_obj_key);
} }


/**
 * Capability space template
 *
 * The capability space of core and non-core components differ in two ways.
 *
 * First, core must keep track of all capabilities of the system. Hence, its
 * capability space must be dimensioned larger.
 *
 * Second, core has to maintain the information about the PD session that
 * was used to allocate the capability to prevent misbehaving clients from
 * freeing capabilities allocated from another component. This information
 * is part of the core-specific 'Native_capability::Data' structure.
 */
template <unsigned NUM_CAPS, typename CAP_DATA>
class Genode::Capability_space_tpl : Noncopyable
{
	private:

		typedef CAP_DATA Data;

		/**
		 * Supplement Native_capability::Data with the meta data needed to
		 * manage it in an AVL tree
		 */
		struct Tree_managed_data : Data, Avl_node<Tree_managed_data>
		{
			template <typename... ARGS>
			Tree_managed_data(ARGS... args) : Data(args...) { }

			Tree_managed_data() { }

			bool higher(Tree_managed_data *data)
			{
				return data->rpc_obj_key().value() > this->rpc_obj_key().value();
			}

			Tree_managed_data *find_by_key(Rpc_obj_key key)
			{
				if (key.value() == this->rpc_obj_key().value()) return this;

				Tree_managed_data *data =
					this->child(key.value() > this->rpc_obj_key().value());

				return data ? data->find_by_key(key) : nullptr;
			}
		};

		Tree_managed_data           _caps_data[NUM_CAPS];
		Bit_allocator<NUM_CAPS>     _alloc { };
		Avl_tree<Tree_managed_data> _tree  { };
		Mutex               mutable _mutex { };

		/**
		 * Calculate index into _caps_data for capability data object
		 */
		unsigned _index(Data const &data) const
		{
			addr_t const offset = (addr_t)&data - (addr_t)_caps_data;
			return offset / sizeof(_caps_data[0]);
		}

		Data *_lookup_unsynchronized(Rpc_obj_key key) const
		{
			/* omit lookup of reply capabilities as they are always foreign */
			if (!key.valid())
				return nullptr;

			if (!_tree.first())
				return nullptr;

			return _tree.first()->find_by_key(key);
		}

		/**
		 * Create Genode capability
		 *
		 * The arguments are passed to the constructor of the
		 * 'Native_capability::Data' type.
		 */
		template <typename... ARGS>
		Native_capability::Data &_create_capability_unsynchronized(ARGS &&... args)
		{
			addr_t const index = _alloc.alloc();

			Tree_managed_data &data = _caps_data[index];

			construct_at<Tree_managed_data>(&data, args...);

			/*
			 * Register capability in the tree only if it refers to a valid
			 * object hosted locally within the component (not foreign).
			 */
			if (data.rpc_obj_key().valid() && !data.dst.foreign)
				_tree.insert(&data);

			return data;
		}

	public:

		void dec_ref(Data &data)
		{
			Mutex::Guard guard(_mutex);

			if (data.dec_ref() > 0)
				return;

			/*
			 * Reference count reached zero. Release the socket descriptors
			 * of the capability-space entry and mark the entry as free.
			 */

			if (data.rpc_obj_key().valid() && !data.dst.foreign)
				_tree.remove(static_cast<Tree_managed_data *>(&data));

			if (data.dst.socket.valid()) {
				lx_close(data.dst.socket.value);

				/*
				 * Close local socketpair end of a locally-implemented RPC
				 * object.
				 */
				if (!data.dst.foreign)
					lx_close(data.rpc_obj_key().value());

			}

			int const index = _index(data);

			_caps_data[index].dst = Rpc_destination::invalid();
			_alloc.free(index);

			data = Tree_managed_data();
		}

		void inc_ref(Data &data)
		{
			Mutex::Guard guard(_mutex);

			if (data.inc_ref() == 255)
				throw Native_capability::Reference_count_overflow();
		}

		Rpc_obj_key rpc_obj_key(Data const &data) const
		{
			return data.rpc_obj_key();
		}

		void print(Output &out, Data const &data) const
		{
			ipc_cap_data(data).print(out);
			Genode::print(out, ",index=", _index(data));
		}

		Capability_space::Ipc_cap_data ipc_cap_data(Data const &data) const
		{
			return { data.dst, data.rpc_obj_key() };
		}

		Native_capability lookup(Rpc_obj_key)
		{
			/*
			 * This method is never called on base-linux. It merely exists
			 * for the compatiblity with the generic 'capability_space.cc'.
			 */
			struct Unreachable { };
			throw Unreachable();
		}

		Native_capability import(Rpc_destination dst, Rpc_obj_key key)
		{
			Data *data_ptr = nullptr;

			{
				Mutex::Guard guard(_mutex);

				data_ptr = _lookup_unsynchronized(key);

				if (data_ptr && !data_ptr->dst.foreign) {

					/*
					 * Compare if existing and incoming sockets refer to the
					 * same inode. If yes, they refer to an RPC object hosted
					 * in the local component. In this case, discard the
					 * incoming socket and keep using the original one.
					 */
					if (data_ptr->dst.socket.inode() == dst.socket.inode()) {
						lx_close(dst.socket.value);

					} else {

						/* force occupation of new capability slot */
						data_ptr = nullptr;
					}
				}

				if (data_ptr == nullptr)
					data_ptr = &_create_capability_unsynchronized(dst, key);
			}

			/* 'Native_capability' constructor aquires '_mutex' via 'inc_ref' */
			return Native_capability(data_ptr);
		}
};


namespace Genode { static inline Rpc_destination invalid_rpc_destination(); }


/* for compatiblity with generic 'capability_space.cc' */
static inline Genode::Rpc_destination Genode::invalid_rpc_destination()
{
	return Rpc_destination::invalid();
}

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_ */
