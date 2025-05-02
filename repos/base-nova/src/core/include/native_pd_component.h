/*
 * \brief  Kernel-specific part of the PD-session interface
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_
#define _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_

/* Genode includes */
#include <nova_native_pd/nova_native_pd.h>

/* core includes */
#include <rpc_cap_factory.h>

namespace Core {

	class Pd_session_component;
	class Native_pd_component;
}


class Core::Native_pd_component : public Rpc_object<Pd_session::Native_pd>
{
	private:

		Pd_session_component &_pd_session;

	public:

		Native_pd_component(Pd_session_component &pd, char const *args);

		~Native_pd_component();

		/**
		 * Native_pd interface
		 */
		Alloc_rpc_cap_result alloc_rpc_cap(Native_capability, addr_t, addr_t) override;

		void imprint_rpc_cap(Native_capability, unsigned long) override;
};

#endif /* _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_ */
