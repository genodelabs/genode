/*
 * \brief  Client-side regulator interface
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2012-02-15
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CLIENT_H_
#define _INCLUDE__REGULATOR_SESSION__CLIENT_H_

#include <regulator_session/capability.h>
#include <base/rpc_client.h>

namespace Regulator {

	class Session_client : public Genode::Rpc_client<Session>
	{
		private:

		public:
			explicit Session_client(Session_capability session)
			: Genode::Rpc_client<Session>(session)
			{ }

			bool enable(unsigned regulator_id) {
				return call<Rpc_enable>(regulator_id);
			}
		
			bool disable(unsigned regulator_id) {
				return call<Rpc_disable>(regulator_id);
			}

			bool is_enabled(unsigned regulator_id) {
				return call<Rpc_is_enabled>(regulator_id);
			}

			enum regulator_state get_state(unsigned regulator_id) {
				return call<Rpc_get_state>(regulator_id);
			}

			bool set_state(unsigned regulator_id, enum regulator_state state) {
				return call<Rpc_set_state>(regulator_id, state);
			}

			unsigned min_level(unsigned regulator_id) {
				return call<Rpc_min_level>(regulator_id);
			}

			unsigned num_level_steps(unsigned regulator_id) {
				return call<Rpc_num_level_steps>(regulator_id);
			}

			int get_level(unsigned regulator_id) {
				return call<Rpc_get_level>(regulator_id);
			}

			bool set_level(unsigned regulator_id, unsigned min, unsigned max)
			{
				return call<Rpc_set_level>(regulator_id, min, max);
			}
	};
}

#endif /* _INCLUDE__REGULATOR_SESSION__CLIENT_H_ */
