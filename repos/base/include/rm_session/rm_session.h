/*
 * \brief  Region-map session interface
 * \author Norman Feske
 * \date   2016-04-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RM_SESSION__RM_SESSION_H_
#define _INCLUDE__RM_SESSION__RM_SESSION_H_

#include <region_map/region_map.h>
#include <session/session.h>

namespace Genode { struct Rm_session; }


struct Genode::Rm_session : Session
{
	static const char *service_name() { return "RM"; }

	/**
	 * Exception types
	 *
	 * \deprecated  The following type definitions will be removed after the
	 *              transition to the 'Region_map' API is completed.
	 */
	typedef Region_map::Attach_failed   Attach_failed;
	typedef Region_map::Out_of_metadata Out_of_metadata;
	typedef Region_map::Region_conflict Region_conflict;

	/**
	 * Create region map
	 *
	 * \param size  upper bound of region map
	 * \return      region-map capability
	 * \throw       Out_of_metadata
	 */
	virtual Capability<Region_map> create(size_t size) = 0;

	/**
	 * Destroy region map
	 */
	virtual void destroy(Capability<Region_map>) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_create, Capability<Region_map>, create,
	                 GENODE_TYPE_LIST(Out_of_metadata), size_t);
	GENODE_RPC(Rpc_destroy, void, destroy, Capability<Region_map>);

	GENODE_RPC_INTERFACE(Rpc_create, Rpc_destroy);
};

#endif /* _INCLUDE__RM_SESSION__RM_SESSION_H_ */
