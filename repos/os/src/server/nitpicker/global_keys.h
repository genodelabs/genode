/*
 * \brief  Global keys policy
 * \author Norman Feske
 * \date   2013-09-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GLOBAL_KEYS_H_
#define _GLOBAL_KEYS_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include "session_component.h"

namespace Nitpicker { class Global_keys; }


class Nitpicker::Global_keys
{
	private:

		struct Policy
		{
			Session_component *_session = nullptr;

			bool defined() const { return _session != nullptr; }

			void client(Session_component *s) { _session = s; }
		};

		enum { NUM_POLICIES = Input::KEY_MAX + 1 };

		Policy _policies[NUM_POLICIES];

		/**
		 * Lookup policy that matches the specified key name
		 */
		Policy *_lookup_policy(char const *key_name);

		bool _valid(Input::Keycode key) const {
			return key >= 0 && key <= Input::KEY_MAX; }

	public:

		Session_component *global_receiver(Input::Keycode key) {
			return _valid(key) ? _policies[key]._session : 0; }

		void apply_config(Xml_node config, Session_list &session_list);
};

#endif /* _GLOBAL_KEYS_H_ */
