/*
 * \brief  Signal root interface on HW-core
 * \author Martin Stein
 * \date   2012-05-06
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
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

namespace Genode
{
	/**
	 * Provide EP to signal root before it initialises root component
	 */
	class Signal_handler
	{
		enum { STACK_SIZE = 4096 };

		Rpc_entrypoint _entrypoint;

		public:

			/**
			 * Constructor
			 */
			Signal_handler(Cap_session * const c)
			: _entrypoint(c, STACK_SIZE, "signal") { }

			/***************
			 ** Accessors **
			 ***************/

			Rpc_entrypoint * entrypoint() { return &_entrypoint; }
	};

	/**
	 * Provides signal service by managing appropriate sessions to the clients
	 */
	class Signal_root : private Signal_handler,
	                    public  Root_component<Signal_session_component>
	{
		public:

			/**
			 * Constructor
			 *
			 * \param md  Meta-data allocator to be used by root component
			 * \param c   CAP session to be used by the root entrypoint
			 */
			Signal_root(Allocator * const md, Cap_session * const c) :
				Signal_handler(c),
				Root_component<Signal_session_component>(entrypoint(), md)
			{ }

		protected:

			/********************************
			 ** 'Root_component' interface **
			 ********************************/

			Signal_session_component * _create_session(const char * args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").long_value(0);
				return new (md_alloc())
					Signal_session_component(md_alloc(), ram_quota);
			}

			void _upgrade_session(Signal_session_component *s,
			                      const char * args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").long_value(0);
				s->upgrade_ram_quota(ram_quota);
			}
	};
}

#endif /* _CORE__INCLUDE__SIGNAL_ROOT_H_ */

