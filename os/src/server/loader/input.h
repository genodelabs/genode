/*
 * \brief  Virtualized input session
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-09-02
 *
 * This input service implementation is used by the virtualized nitpicker
 * service to translate the input coordinate system from the coordinates
 * seen by the user of the virtualized session and the physical coordinates
 * dictated by the loader-session client.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_H_
#define _INPUT_H_

#include <base/rpc_server.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <input_session/client.h>

namespace Input {

	using namespace Genode;

	struct Transformer
	{
		struct Delta { int const x, y; };

		virtual Delta delta() = 0;
	};

	class Session_component : public Rpc_object<Session>
	{
		private:

			Session_client _real_input;
			Transformer   &_transformer;
			Event  * const _ev_buf;

		public:

			/**
			 * Constructor
			 */
			Session_component(Session_capability real_input,
			                  Transformer       &transformer)
			:
				_real_input(real_input), _transformer(transformer),
				_ev_buf(env()->rm_session()->attach(_real_input.dataspace()))
			{ }

			/**
			 * Destructor
			 */
			~Session_component() { env()->rm_session()->detach(_ev_buf); }


			/*****************************
			 ** Input session interface **
			 *****************************/

			Dataspace_capability dataspace() { return _real_input.dataspace(); }

			bool is_pending() const { return _real_input.is_pending(); }

			int flush()
			{
				/* translate mouse position to child's coordinate system */
				Transformer::Delta delta = _transformer.delta();

				int const num_ev = _real_input.flush();
				for (int i = 0; i < num_ev; i++) {

					Input::Event &ev = _ev_buf[i];

					if ((ev.type()    == Input::Event::MOTION)
					 || (ev.type()    == Input::Event::WHEEL)
					 || (ev.keycode() == Input::BTN_LEFT)
					 || (ev.keycode() == Input::BTN_RIGHT)
					 || (ev.keycode() == Input::BTN_MIDDLE)) {

						ev = Input::Event(ev.type(),
						                  ev.keycode(),
						                  ev.ax() - delta.x,
						                  ev.ay() - delta.y,
						                  ev.rx(),
						                  ev.ry());
					}
				}

				return num_ev;
			}
	};
}

#endif /* _INPUT_H_ */
