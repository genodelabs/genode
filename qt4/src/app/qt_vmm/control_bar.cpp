/*
 * \brief   Control bar
 * \author  Stefan Kalkowski
 * \date    2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <input/keycodes.h>

/* Qoost includes */
#include <qoost/style.h>

#include "main_window.h"


void Control_bar::_pause_resume()
{
	_st_play.submit();
	_playing = !_playing;
	update_style_id(_play_pause_button, _playing ? "play" : "pause");
}

void Control_bar::_stop()
{
	_st_stop.submit();
	_playing = false;
	update_style_id(_play_pause_button, "pause");
}

void Control_bar::_bomb()         { _st_bomb.submit();  }
void Control_bar::_power()        { _st_power.submit(); }

Control_bar::Control_bar()
: _playing(false)
{
	_play_pause_button->setParent(this);
	_stop_button->setParent(this);
	_bomb_button->setParent(this);
	_power_button->setParent(this);

	update_style_id(_play_pause_button, "pause");

	_layout->addStretch();
	_layout->addWidget(_play_pause_button);
	_layout->addWidget(_stop_button);
	_layout->addWidget(_bomb_button);
	_layout->addWidget(_power_button);
	_layout->addStretch();
	_layout->setContentsMargins(3, 3, 3, 3);

	connect(_play_pause_button, SIGNAL(clicked()), this, SLOT(_pause_resume()));
	connect(_stop_button,       SIGNAL(clicked()), this, SLOT(_stop()));
	connect(_bomb_button,       SIGNAL(clicked()), this, SLOT(_bomb()));
	connect(_power_button,      SIGNAL(clicked()), this, SLOT(_power()));
}
