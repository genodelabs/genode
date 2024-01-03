/*
 * \brief  RPC object that owns client-provided RAM and capability quotas
 * \author Norman Feske
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SESSION_OBJECT_H_
#define _INCLUDE__BASE__SESSION_OBJECT_H_

#include <util/arg_string.h>
#include <base/entrypoint.h>
#include <session/session.h>

namespace Genode { template <typename, typename> struct Session_object; }

template <typename RPC_INTERFACE, typename SERVER = RPC_INTERFACE>
class Genode::Session_object : private Ram_quota_guard,
                               private Cap_quota_guard,
                               public  Rpc_object<RPC_INTERFACE, SERVER>
{
	public:

		typedef Session::Label     Label;
		typedef Session::Diag      Diag;
		typedef Session::Resources Resources;

		using Ram_quota_guard::withdraw;
		using Cap_quota_guard::withdraw;
		using Ram_quota_guard::replenish;
		using Cap_quota_guard::replenish;
		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;

	private:

		Rpc_entrypoint &_ep;

		Diag _diag;

	protected:

		Label const _label;

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		/**
		 * Constructor
		 */
		Session_object(Entrypoint &ep, Resources const &resources,
		               Label const &label, Diag diag)
		:
			Session_object(ep.rpc_ep(), resources, label, diag)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated This constructor exists for backward compatibility only
		 *             and will eventually be removed.
		 */
		Session_object(Rpc_entrypoint &ep, Resources const &resources,
		               Label const &label, Diag diag)
		:
			Ram_quota_guard(resources.ram_quota),
			Cap_quota_guard(resources.cap_quota),
			_ep(ep), _diag(diag), _label(label)
		{
			Cap_quota_guard::withdraw(Cap_quota{1});
			_ep.manage(this);
		}

		~Session_object()
		{
			_ep.dissolve(this);
			Cap_quota_guard::replenish(Cap_quota{1});
		}

		/**
		 * Hook called whenever the session quota was upgraded by the client
		 */
		virtual void session_quota_upgraded() { }

		/**
		 * Return client-specific session label
		 */
		Label label() const { return _label; }

		/**
		 * Output label-prefixed diagnostic message conditionally
		 *
		 * The method produces output only if the session is in diagnostic
		 * mode (defined via the 'diag' session argument).
		 */
		template <typename... ARGS>
		void diag(ARGS &&... args) const
		{
			if (_diag.enabled)
				log(RPC_INTERFACE::service_name(), " (", _label, ") ", args...);
		}

		/**
		 * Output label-prefixed error message
		 */
		template <typename... ARGS>
		void error(ARGS &&... args) const
		{
			Genode::error(RPC_INTERFACE::service_name(), " (", _label, ") ", args...);
		}

		/**
		 * Output label-prefixed error message
		 */
		template <typename... ARGS>
		void warning(ARGS &&... args) const
		{
			Genode::warning(RPC_INTERFACE::service_name(), " (", _label, ") ", args...);
		}
};

#endif /* _INCLUDE__BASE__SESSION_OBJECT_H_ */
