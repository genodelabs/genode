/*
 * \brief  D3m NIC service
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NIC_SERVICE_H_
#define _NIC_SERVICE_H_

/* Genode includes */
#include <nic_session/nic_session.h>
#include <root/component.h>
#include <util/list.h>

namespace Nic {

	struct Provider
	{
		bool ready_to_use() { return root().valid(); }

		virtual Genode::Root_capability root() = 0;
	};

	/**
	 * Root interface of NIC service
	 */
	class Root : public Genode::Rpc_object<Genode::Typed_root<Nic::Session> >
	{
		private:

			Provider &_nic_provider;

		public:

			Genode::Session_capability session(Session_args     const &args,
			                                   Genode::Affinity const &affinity)
			{
				if (!args.is_valid_string()) throw Invalid_args();

				if (!_nic_provider.ready_to_use())
					throw Unavailable();

				try {
					return Genode::Root_client(_nic_provider.root())
					       .session(args.string(), affinity);
				} catch (...) {
					throw Unavailable();
				}
			}

			void upgrade(Genode::Session_capability, Upgrade_args const &) { }

			void close(Genode::Session_capability session)
			{
				Genode::Root_client(_nic_provider.root()).close(session);
			}

			Root(Provider &nic_provider) : _nic_provider(nic_provider) { }
	};
}

#endif /* _INPUT_SERVICE_H_ */
