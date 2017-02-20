/*
 * \brief  Local capability
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-05-22
 *
 * A typed capability is a capability tied to one specifiec RPC interface
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCAL_CAPABILITY_H_
#define _INCLUDE__BASE__INTERNAL__LOCAL_CAPABILITY_H_

#include <base/internal/capability_space_tpl.h>

namespace Genode {
	template <typename> class Local_capability;

	/**
	 * Return true if argument is a local capability
	 */
	static inline bool local(Untyped_capability const &cap)
	{
		return Capability_space::ipc_cap_data(cap).dst.socket == -1;
	}
}


/**
 * Local capability referring to a specific RPC interface
 *
 * \param RPC_INTERFACE  class containing the RPC interface declaration
 */
template <typename RPC_INTERFACE>
class Genode::Local_capability
{
	public:

		/**
		 * Factory method to construct a local-capability.
		 *
		 * Local-capabilities can be used protection-domain internally
		 * only. They simply incorporate a pointer to some process-local
		 * object.
		 *
		 * \param ptr pointer to the corresponding local object.
		 * \return a capability that represents the local object.
		 */
		static Capability<RPC_INTERFACE> local_cap(RPC_INTERFACE* ptr) {
			Untyped_capability cap =
				Capability_space::import(invalid_rpc_destination(),
				                         Rpc_obj_key((long)ptr));
			return reinterpret_cap_cast<RPC_INTERFACE>(cap); }

		/**
		 * Dereference a local-capability.
		 *
		 * \param c the local-capability.
		 * \return pointer to the corresponding local object.
		 */
		static RPC_INTERFACE* deref(Capability<RPC_INTERFACE> c) {
			return reinterpret_cast<RPC_INTERFACE*>(c.local_name()); }
};

#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_CAPABILITY_H_ */
