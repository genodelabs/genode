/*
 * \brief  Capability-space management for traditional L4 kernels and Linux
 * \author Norman Feske
 * \date   2016-06-15
 *
 * On traditional L4 kernels, a capability is represented by the thread ID
 * of the invoked entrypoint thread and a globally unique RPC object key.
 * On Linux, a capability is represented by a socket descriptor and an RPC
 * object key. The thread ID respectively socket descriptor refer to the
 * recipient of an RPC call (RPC destination).
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_

/* base includes */
#include <util/avl_tree.h>
#include <util/bit_allocator.h>
#include <base/lock.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/capability_space.h>
#include <base/internal/rpc_destination.h>

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
class Genode::Capability_space_tpl
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
				return data->rpc_obj_key().value() > rpc_obj_key().value();
			}

			Tree_managed_data *find_by_key(Rpc_obj_key key)
			{
				if (key.value() == rpc_obj_key().value()) return this;

				Tree_managed_data *data =
					this->child(key.value() > rpc_obj_key().value());

				return data ? data->find_by_key(key) : nullptr;
			}
		};

		Tree_managed_data           _caps_data[NUM_CAPS];
		Bit_allocator<NUM_CAPS>     _alloc;
		Avl_tree<Tree_managed_data> _tree;
		Lock                mutable _lock;

		/**
		 * Calculate index into _caps_data for capability data object
		 */
		unsigned _index(Data const &data) const
		{
			addr_t const offset = (addr_t)&data - (addr_t)_caps_data;
			return offset / sizeof(_caps_data[0]);
		}

		Data *_lookup(Rpc_obj_key key) const
		{
			Lock::Guard guard(_lock);

			if (!_tree.first())
				return nullptr;

			return _tree.first()->find_by_key(key);
		}

	public:

		/**
		 * Create Genode capability
		 *
		 * The arguments are passed to the constructor of the
		 * 'Native_capability::Data' type.
		 */
		template <typename... ARGS>
		Native_capability::Data &create_capability(ARGS... args)
		{
			Lock::Guard guard(_lock);

			addr_t const index = _alloc.alloc();

			_caps_data[index] = Tree_managed_data(args...);

			if (_caps_data[index].rpc_obj_key().valid())
				_tree.insert(&_caps_data[index]);

			return _caps_data[index];
		}

		void dec_ref(Data &data)
		{
			Lock::Guard guard(_lock);

			if (data.dec_ref() == 0) {

				if (data.rpc_obj_key().valid())
					_tree.remove(static_cast<Tree_managed_data *>(&data));

				_alloc.free(_index(data));
				data = Tree_managed_data();
			}
		}

		void inc_ref(Data &data)
		{
			Lock::Guard guard(_lock);

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
		}

		Capability_space::Ipc_cap_data ipc_cap_data(Data const &data) const
		{
			if (&data == nullptr) {
				raw("ipc_cap_data nullptr");
				for (;;);
			}
			return { data.dst, data.rpc_obj_key() };
		}

		Native_capability lookup(Rpc_obj_key rpc_obj_key)
		{
			Native_capability::Data *data = _lookup(rpc_obj_key);
			return data ? Native_capability(*data) : Native_capability();
		}

		Native_capability import(Rpc_destination dst, Rpc_obj_key key)
		{
			return Native_capability(create_capability(dst, key));
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_TPL_H_ */
