/*
 * \brief  seL4-specific capability-space management
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_
#define _BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_

/* base includes */
#include <util/avl_tree.h>
#include <base/lock.h>

/* base-internal includes */
#include <internal/capability_space.h>
#include <internal/assert.h>

namespace Genode { template <unsigned, typename> class Capability_space_sel4; }


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
		unsigned    sel;
	};

	/**
	 * Retrieve IPC data for given capability
	 */
	Ipc_cap_data ipc_cap_data(Native_capability const &cap);

	/**
	 * Allocate unused selector for receiving a capability via IPC
	 */
	unsigned alloc_rcv_sel();

} }


/**
 * Capability space template
 *
 * The capability space of core and non-core components differ in two ways.
 *
 * First, core must keep track of all capabilities of the system. Hence, its
 * capability space must be dimensioned larger.
 *
 * Second, core has to maintain the information about the CAP session that
 * was used to allocate the capability to prevent misbehaving clients from
 * freeing capabilities allocated from another component. This information
 * is part of the core-specific 'Native_capability::Data' structure.
 */
template <unsigned NUM_CAPS, typename CAP_DATA>
class Genode::Capability_space_sel4
{
	public:

		/*
		 * The capability space consists of two parts. The lower part is
		 * populated with statically-defined capabilities whereas the upper
		 * part is dynamically managed by the component. The 'NUM_STATIC_CAPS'
		 * defines the size of the first part.
		 */
		enum { NUM_STATIC_CAPS = 1024 };

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
		Lock                        _lock;

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
		bool _is_dynamic(Data &data) const
		{
			return _index(data) >= NUM_STATIC_CAPS;
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
		Native_capability::Data &create_capability(unsigned sel, ARGS... args)
		{
			Lock::Guard guard(_lock);

			ASSERT(!_caps_data[sel].rpc_obj_key().valid());
			ASSERT(sel < NUM_CAPS);

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

			if (_is_dynamic(data) && !data.dec_ref()) {
				PDBG("remove cap");
				_remove(data);
			}
		}

		void inc_ref(Data &data)
		{
			Lock::Guard guard(_lock);

			if (_is_dynamic(data))
				data.inc_ref();
		}

		Rpc_obj_key rpc_obj_key(Data const &data) const
		{
			return data.rpc_obj_key();
		}

		Capability_space::Ipc_cap_data ipc_cap_data(Data const &data) const
		{
			return { rpc_obj_key(data), sel(data) };
		}
};


#endif /* _BASE__INTERNAL__CAPABILITY_SPACE_SEL4_H_ */
