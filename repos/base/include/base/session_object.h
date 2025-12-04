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

		using Label     = Session::Label;
		using Resources = Session::Resources;

		using Ram_quota_guard::try_withdraw;
		using Cap_quota_guard::try_withdraw;
		using Ram_quota_guard::replenish;
		using Cap_quota_guard::replenish;
		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;

	private:

		Rpc_entrypoint &_ep;

	protected:

		Label const _label;

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		/**
		 * Constructor
		 */
		Session_object(Entrypoint &ep, Resources const &res, Label const &label)
		:
			Session_object(ep.rpc_ep(), res, label)
		{ }

		/**
		 * Constructor used by core
		 */
		Session_object(Rpc_entrypoint &ep, Resources const &res, Label const &label)
		:
			Ram_quota_guard(res.ram_quota),
			Cap_quota_guard(res.cap_quota),
			_ep(ep), _label(label)
		{
			if (Cap_quota_guard::try_withdraw(Cap_quota{1}))
				_ep.manage(this);
			else
				error("insufficient cap quota for session-object creation");
		}

		~Session_object()
		{
			if (Rpc_object<RPC_INTERFACE, SERVER>::cap().valid()) {
				_ep.dissolve(this);
				Cap_quota_guard::replenish(Cap_quota{1});
			}
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
		 * Output label-prefixed error message
		 */
		void error(auto &&... args) const
		{
			Genode::error(RPC_INTERFACE::service_name(), " (", _label, ") ", args...);
		}

		/**
		 * Output label-prefixed error message
		 */
		void warning(auto &&... args) const
		{
			Genode::warning(RPC_INTERFACE::service_name(), " (", _label, ") ", args...);
		}
};

#endif /* _INCLUDE__BASE__SESSION_OBJECT_H_ */
