/*
 * \brief  Component-local implementation of the parent interface
 * \author Norman Feske
 * \date   2016-04-29
 *
 * On Linux, we intercept the parent interface to implement services that are
 * concerned with virtual-memory management locally within the component.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCAL_PARENT_H_
#define _INCLUDE__BASE__INTERNAL__LOCAL_PARENT_H_

/* Genode includes */
#include <base/allocator.h>

/* base-internal includes */
#include <base/internal/expanding_parent_client.h>

namespace Genode {

	class Local_session;
	class Local_parent;
}


/**
 * Local interceptor of parent requests
 *
 * On Linux, we need to intercept calls to the parent interface to implement
 * the RM service locally. This particular service is used for creating managed
 * dataspaces, which allow the reservation of parts of the local address space
 * from being automatically managed by the 'env()->rm_session()'.
 *
 * All requests that do not refer to the RM service are passed through the real
 * parent interface.
 */
class Genode::Local_parent : public Expanding_parent_client
{
	private:

		Allocator &_alloc;
		Id_space<Client> _local_sessions_id_space;

	public:

		/**********************
		 ** Parent interface **
		 **********************/

		Session_capability session(Client::Id, Service_name const &, Session_args const &,
		                           Affinity const & = Affinity()) override;
		Close_result close(Client::Id) override;

		/**
		 * Constructor
		 *
		 * \param parent_cap  real parent capability used to
		 *                    promote requests to non-local
		 *                    services
		 */
		Local_parent(Parent_capability parent_cap, Allocator &);
};

#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_PARENT_H_ */
