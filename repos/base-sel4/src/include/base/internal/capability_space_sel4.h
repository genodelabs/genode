/*
 * \brief  seL4-specific capability-space management
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_

/* base includes */
#include <util/avl_tree.h>
#include <base/lock.h>

/* base-internal includes */
#include <base/internal/capability_space.h>
#include <base/internal/assert.h>

namespace Genode {
	
	template <unsigned, unsigned, typename> class Capability_space_sel4;

	class Cap_sel
	{
		private:

			addr_t _value;

		public:

			explicit Cap_sel(addr_t value) : _value(value) { }

			addr_t value() const { return _value; }

			void print(Output &out) const { Genode::print(out, "sel=", _value); }
	};
}


/**
 * Platform-specific supplement to the generic 'Capability_space' interface
 */
namespace Genode { namespace Capability_space {

	/**
	 * Information needed to transfer capability via the kernel's IPC mechanism
	 */
	struct Ipc_cap_data
	{
		Rpc_obj_key rpc_obj_key;
		Cap_sel     sel;

		Ipc_cap_data(Rpc_obj_key rpc_obj_key, unsigned sel)
		: rpc_obj_key(rpc_obj_key), sel(sel) { }

		void print(Output &out) const { Genode::print(out, sel, ",", rpc_obj_key); }
	};

	/**
	 * Retrieve IPC data for given capability
	 */
	Ipc_cap_data ipc_cap_data(Native_capability const &cap);

	/**
	 * Allocate unused selector for receiving a capability via IPC
	 */
	unsigned alloc_rcv_sel();

	/**
	 * Delete selector but retain allocation
	 *
	 * This function is used when a delegated capability selector is replaced
	 * with an already known selector. The delegated selector is discarded.
	 */
	void reset_sel(unsigned sel);

	/**
	 * Lookup capability by its RPC object key
	 */
	Native_capability lookup(Rpc_obj_key key);

	/**
	 * Import capability into the component's capability space
	 */
	Native_capability import(Ipc_cap_data ipc_cap_data);
} }


namespace Genode
{
	enum {
		INITIAL_SEL_LOCK   = 0,
		INITIAL_SEL_PARENT = 1,
		INITIAL_SEL_CNODE  = 2,
		INITIAL_SEL_END
	};

	enum {
		CSPACE_SIZE_LOG2_1ST      = 6,
		CSPACE_SIZE_LOG2_2ND      = 8,
		CSPACE_SIZE_LOG2          = CSPACE_SIZE_LOG2_1ST + CSPACE_SIZE_LOG2_2ND,
		NUM_CORE_MANAGED_SEL_LOG2 = 8,
	};
};


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
template <unsigned NUM_CAPS, unsigned _NUM_CORE_MANAGED_CAPS, typename CAP_DATA>
class Genode::Capability_space_sel4
{
	public:

		/*
		 * The capability space consists of two parts. The lower part is
		 * populated with statically-defined capabilities whereas the upper
		 * part is dynamically managed by the component. The
		 * 'NUM_CORE_MANAGED_CAPS' defines the size of the first part.
		 */
		enum { NUM_CORE_MANAGED_CAPS = _NUM_CORE_MANAGED_CAPS };

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

		/**
		 * Return true if capability is locally managed by the component
		 */
		bool _is_core_managed(Data &data) const
		{
			return _index(data) < NUM_CORE_MANAGED_CAPS;
		}

		void _remove(Native_capability::Data &data)
		{
			if (_caps_data[_index(data)].rpc_obj_key().valid())
				_tree.remove(static_cast<Tree_managed_data *>(&data));

			_caps_data[_index(data)] = Tree_managed_data();
		}

	public:

		/*****************************************************
		 ** Support for the Core_capability_space interface **
		 *****************************************************/

		/**
		 * Create Genode capability for kernel cap selector 'sel'
		 *
		 * The arguments following the selector are passed to the constructor
		 * of the 'Native_capability::Data' type.
		 */
		template <typename... ARGS>
		Native_capability::Data &create_capability(Cap_sel cap_sel, ARGS... args)
		{
			addr_t const sel = cap_sel.value();

			ASSERT(sel < NUM_CAPS);
			ASSERT(!_caps_data[sel].rpc_obj_key().valid());

			Lock::Guard guard(_lock);

			_caps_data[sel] = Tree_managed_data(args...);

			if (_caps_data[sel].rpc_obj_key().valid())
				_tree.insert(&_caps_data[sel]);

			return _caps_data[sel];
		}

		/**
		 * Return kernel cap selector
		 */
		unsigned sel(Data const &data) const { return _index(data); }


		/************************************************
		 ** Support for the Capability_space interface **
		 ************************************************/

		void dec_ref(Data &data)
		{
			Lock::Guard guard(_lock);

			if (!_is_core_managed(data) && !data.dec_ref())
				_remove(data);
		}

		void inc_ref(Data &data)
		{
			Lock::Guard guard(_lock);

			if (!_is_core_managed(data)) {
				data.inc_ref();
			}
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
			return { rpc_obj_key(data), sel(data) };
		}

		Data *lookup(Rpc_obj_key key) const
		{
			Lock::Guard guard(_lock);

			if (!_tree.first())
				return nullptr;

			return _tree.first()->find_by_key(key);
		}
};


#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_ */
