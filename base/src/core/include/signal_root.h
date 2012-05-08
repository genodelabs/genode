/*
 * \brief  Signal root interface
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SIGNAL_ROOT_H_
#define _CORE__INCLUDE__SIGNAL_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* core includes */
#include <signal_session_component.h>

namespace Genode {

	class Signal_handler
	{
		private:

			enum { STACK_SIZE = sizeof(addr_t)*1024 };
			Rpc_entrypoint _ep;

		public:

			Signal_handler(Cap_session *cap_session) :
				_ep(cap_session, STACK_SIZE, "signal")
			{ }

			Rpc_entrypoint *entrypoint() { return &_ep; }
	};

	class Signal_root : private Signal_handler,
	                    public Root_component<Signal_session_component>
	{
		protected:

			Signal_session_component *_create_session(const char *args)
			{
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").long_value(0);
				return new (md_alloc())
					Signal_session_component(entrypoint(), entrypoint(),
					                         md_alloc(), ram_quota);
			}

			void _upgrade_session(Signal_session_component *s, const char *args)
			{
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").long_value(0);
				s->upgrade_ram_quota(ram_quota);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Signal_root(Allocator *md_alloc, Cap_session *cap_session)
			:
				Signal_handler(cap_session),
				Root_component<Signal_session_component>(entrypoint(), md_alloc)
			{ }
	};
}

#endif /* _CORE__INCLUDE__SIGNAL_ROOT_H_ */
