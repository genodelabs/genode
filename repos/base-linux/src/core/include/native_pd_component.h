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
#include <linux_native_pd/linux_native_pd.h>

namespace Genode {

	class Dataspace_component;
	class Pd_session_component;
	class Native_pd_component;
}


class Genode::Native_pd_component : public Rpc_object<Linux_native_pd,
	                                                  Native_pd_component>
{
	private:

		enum { LABEL_MAX_LEN     = 1024 };
		enum { ROOT_PATH_MAX_LEN =  512 };

		Pd_session_component &_pd_session;
		char                  _root[ROOT_PATH_MAX_LEN];
		unsigned long         _pid = 0;
		unsigned              _uid = 0;
		unsigned              _gid = 0;

		void _start(Dataspace_component &ds);

	public:

		Native_pd_component(Pd_session_component &pd, char const *args);

		~Native_pd_component();

		void start(Capability<Dataspace> binary);
};

#endif /* _CORE__INCLUDE__NATIVE_PD_COMPONENT_H_ */
