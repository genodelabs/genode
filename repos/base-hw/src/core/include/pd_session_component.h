/*
 * \brief  Core-specific instance of the PD session interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <pd_session/pd_session.h>
#include <util/arg_string.h>

/* core includes */
#include <platform_pd.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Rpc_object<Pd_session>
{
	private:

		/**
		 * Read and store the PD label
		 */
		struct Label {

			enum { MAX_LEN = 64 };
			char string[MAX_LEN];

			Label(char const *args)
			{
				Arg_string::find_arg(args, "label").string(string,
				                                           sizeof(string), "");
			}
		} const _label;

		Allocator_guard    _md_alloc;   /* guarded meta-data allocator */
		Platform_pd        _pd;
		Parent_capability  _parent;
		Rpc_entrypoint    *_thread_ep;

		size_t _ram_quota(char const * args) {
			return Arg_string::find_arg(args, "ram_quota").long_value(0); }

	public:

		Pd_session_component(Rpc_entrypoint * thread_ep,
		                     Allocator      * md_alloc,
		                     char const     * args)
		: _label(args),
		  _md_alloc(md_alloc, _ram_quota(args)),
		  _pd(&_md_alloc, _label.string),
		  _thread_ep(thread_ep) { }

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota)
		{
			_md_alloc.upgrade(ram_quota);
			_pd.upgrade_slab(_md_alloc);
		}


		/**************************/
		/** PD session interface **/
		/**************************/

		int bind_thread(Thread_capability);
		int assign_parent(Parent_capability);
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
