/*
 * \brief   Control bar
 * \author  Christian Prochaska
 * \date    2012-03-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <input/keycodes.h>

/* Qoost includes */
#include <qoost/style.h>

/* local includes */
#include "main_window.h"


void Control_bar::_rewind()
{
	/* mouse click at horizontal position 0 */
	_input.submit(Input::Event(Input::Event::PRESS, Input::BTN_LEFT, 0, 0, 0, 0));
	_input.submit(Input::Event(Input::Event::RELEASE, Input::BTN_LEFT, 0, 0, 0, 0));
}


void Control_bar::_pause_resume()
{
	_input.submit(Input::Event(Input::Event::PRESS, Input::KEY_SPACE, 0, 0, 0, 0));
	_input.submit(Input::Event(Input::Event::RELEASE, Input::KEY_SPACE, 0, 0, 0, 0));

	_playing = !_playing;
	if (_playing)
		update_style_id(_play_pause_button, "play");
	else
		update_style_id(_play_pause_button, "pause");
}


void Control_bar::_stop()
{
	if (_playing)
		_pause_resume();

	_rewind();
}


Control_bar::Control_bar(Input::Session_component &input)
:
	_input(input), _playing(true)
{
	update_style_id(_play_pause_button, "play");

	_volume_slider->setOrientation(Qt::Horizontal);
	_volume_slider->setRange(0, 100);
	_volume_slider->setTickInterval(10);
	_volume_slider->setValue(100);

	_layout->addWidget(_play_pause_button);
	_layout->addWidget(_stop_button);
	_layout->addStretch();
	_layout->addWidget(_volume_label);
	_layout->addWidget(_volume_slider);

	connect(_play_pause_button, SIGNAL(clicked()), this, SLOT(_pause_resume()));
	connect(_stop_button, SIGNAL(clicked()), this, SLOT(_stop()));
	connect(_volume_slider, SIGNAL(valueChanged(int)), this, SIGNAL(volume_changed(int)));
}
