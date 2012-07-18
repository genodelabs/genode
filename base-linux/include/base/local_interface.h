/*
 * \brief  Support for process-local pseudo capabilities
 * \author Norman Feske
 * \date   2011-11-21
 *
 * Pseudo capabilities have a zero 'tid' and a non-zero 'local_name'. The local
 * name is a pointer to the local object implementing the interface. Pseudo
 * capabilties are valid only as arguments for local services that are prepared
 * for it. I.e., the locally implemented RM service accepts pseudo dataspace
 * capabilities that refer to managed dataspaces. Or the Linux-specific
 * 'Rm_session_client' takes a pseudo capability to target RM-session
 * invokations to the local implementation.
 *
 * Please note that this header file is not part of the official Genode API.
 * It exists on no other platform than Linux and is meant for Genode-internal
 * use only.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__LOCAL_INTERFACE_H_
#define _INCLUDE__BASE__LOCAL_INTERFACE_H_

#include <base/capability.h>
#include <base/printf.h>

namespace Genode {

	/**
	 * Common base class of local interface implementations
	 */
	struct Local_interface
	{
		virtual ~Local_interface() { }

		/**
		 * Exception type
		 */
		class Non_local_capability { };

		/**
		 * Convert pseudo capability to pointer to locally implemented session
		 *
		 * \param IF   interface type
		 * \param cap  pseudo capability
		 *
		 * \throw Non_local_capability  if the argument does not refer to a
		 *                              locally implemented interface
		 */
		template <typename IF>
		static IF *deref(Capability<IF> cap)
		{
			/* check if this is a pseudo capability */
			if (cap.dst().tid != 0 || !cap.local_name())
				throw Non_local_capability();

			/*
			 * For a pseudo capability, the 'local_name' points to the local
			 * session object of the correct type.
			 */
			IF *interface = dynamic_cast<IF *>((Local_interface *)cap.local_name());
			if (!interface)
				throw Non_local_capability();

			return interface;
		}

		/**
		 * Construct pseudo capability to process-local interface implementation
		 *
		 * \param IF         interface type
		 * \param interface  pointer to local interface implementation
		 * \return           pseudo capability
		 *
		 */
		template <typename IF>
		static Capability<IF> capability(IF *interface)
		{
			typedef Native_capability::Dst Dst;
			return reinterpret_cap_cast<IF>(Native_capability(Dst(), (long)interface));
		};
	};
}

#endif /* _INCLUDE__BASE__LOCAL_INTERFACE_H_ */
