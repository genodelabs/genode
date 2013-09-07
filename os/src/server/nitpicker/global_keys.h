/*
 * \brief  Global keys policy
 * \author Norman Feske
 * \date   2013-09-06
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GLOBAL_KEYS_H_
#define _GLOBAL_KEYS_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include "session.h"

class Global_keys
{
	private:

		struct Policy
		{
			enum Type {

				/**
				 * Key is not global but should be propagated to focused client
				 */
				UNDEFINED,

				/**
				 * Key activates nitpicker's built-in kill mode
				 */
				KILL,

				/**
				 * Key activates nitpicker's built-in X-ray mode
				 */
				XRAY,

				/**
				 * Key should be propagated to client session
				 */
				CLIENT
			};

			Type     _type;
			Session *_session;

			Policy() : _type(UNDEFINED), _session(0) { }

			void undefine()         { _type = UNDEFINED; _session = 0; }
			void operation_kill()   { _type = KILL;      _session = 0; }
			void operation_xray()   { _type = XRAY;      _session = 0; }
			void client(Session *s) { _type = CLIENT;    _session = s; }

			bool defined() const { return _type != UNDEFINED; }
			bool xray()    const { return _type == XRAY; }
			bool kill()    const { return _type == KILL; }
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

		Session *global_receiver(Input::Keycode key) {
			return _valid(key) ? _policies[key]._session : 0; }

		void apply_config(Session_list &session_list);

		bool is_operation_key(Input::Keycode key) const {
			return _valid(key) && (_policies[key].xray() || _policies[key].kill()); }

		bool is_xray_key(Input::Keycode key) const {
			return _valid(key) && _policies[key].xray(); }

		bool is_kill_key(Input::Keycode key) const {
			return _valid(key) && _policies[key].kill(); }
};

#endif /* _GLOBAL_KEYS_H_ */
