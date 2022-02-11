/*
 * \brief  Testing the functionality of the black hole component
 * \author Martin Stein
 * \date   2022-02-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>

/* os includes */
#include <event_session/connection.h>
#include <capture_session/connection.h>
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>

#include <input/keycodes.h>

using namespace Genode;

namespace Test {

	class Main;
}


class Test::Main
{
	private:

		Env                         &_env;
		Event::Connection            _event               { _env };
		Capture::Connection          _capture             { _env };
		Capture::Area                _capture_screen_size { 1, 1 };
		Capture::Pixel               _capture_pixels[1];
		Surface<Capture::Pixel>      _capture_surface     { _capture_pixels, _capture_screen_size };
		Capture::Connection::Screen  _capture_screen      { _capture, _env.rm(), _capture_screen_size };
		Audio_in::Connection         _audio_in            { _env, "left" };
		Audio_out::Connection        _audio_out           { _env, "left" };

	public:

		Main(Env &env) : _env { env }
		{
			_event.with_batch([&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Press {Input::KEY_1 });
				batch.submit(Input::Release {Input::KEY_2 });
				batch.submit(Input::Relative_motion { 3, 4 });
			});

			_capture_screen.apply_to_surface(_capture_surface);

			log("Finished");
		}
};


void Component::construct(Env &env)
{
	static Test::Main main { env };
}
