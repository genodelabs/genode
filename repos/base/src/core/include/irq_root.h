/*
 * \brief  IRQ root interface
 * \author Christian Helmuth
 * \date   2007-09-13
 *
 * FIXME locking
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_ROOT_H_
#define _CORE__INCLUDE__IRQ_ROOT_H_

/* Genode includes */
#include <util/arg_string.h>
#include <base/allocator.h>
#include <root/root.h>
#include <base/rpc_server.h>

/* core includes */
#include <irq_session_component.h>

namespace Genode {

	class Irq_root : public Rpc_object<Typed_root<Irq_session> >
	{

		private:

			Cap_session                *_cap_session;
			Range_allocator            *_irq_alloc;    /* platform irq allocator */
			Allocator                  *_md_alloc;     /* meta-data allocator */
			List<Irq_session_component> _sessions;     /* started irq sessions */

		public:

			/**
			 * Constructor
			 *
			 * \param cap_session capability allocator
			 * \param irq_alloc   IRQ range that can be assigned to clients
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Irq_root(Cap_session     *cap_session,
			         Range_allocator *irq_alloc,
			         Allocator       *md_alloc)
			: _cap_session(cap_session), _irq_alloc(irq_alloc), _md_alloc(md_alloc) { }


			/********************
			 ** Root interface **
			 ********************/

			Session_capability session(Session_args const &args, Affinity const &affinity)
			{
				if (!args.is_valid_string()) throw Invalid_args();

				/*
				 * We need to decrease 'ram_quota' by
				 * the size of the session object.
				 */
				size_t ram_quota = Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);
				long remaining_ram_quota = ram_quota - sizeof(Irq_session_component) -
				                           _md_alloc->overhead(sizeof(Irq_session_component));
				if (remaining_ram_quota < 0) {
					PERR("Insufficient ram quota, provided=%zu, required=%zu",
					     ram_quota, sizeof(Irq_session_component) +
					                _md_alloc->overhead(sizeof(Irq_session_component)));
					return Session_capability();
				}

				Irq_session_component *s;
				try {
					s = new (_md_alloc) Irq_session_component(_cap_session, _irq_alloc, args.string());
				} catch (Allocator::Out_of_memory) { return Session_capability(); }

				if (!s->cap().valid())
					return Session_capability();

				_sessions.insert(s);

				return s->cap();
			}

			void upgrade(Session_capability, Upgrade_args const &)
			{
				/* there is no need to upgrade an IRQ session */
			}

			void close(Session_capability session)
			{
				Irq_session_component *s = _sessions.first();

				for (; s; s = s->next()) {
					if (s->cap().local_name() == session.local_name())
					 	break;
				}
				if (!s) return;

				_sessions.remove(s);

				/* XXX Currently we support only one client... */
				destroy(_md_alloc, s);
			}
	};
}

#endif /* _CORE__INCLUDE__IRQ_ROOT_H_ */
