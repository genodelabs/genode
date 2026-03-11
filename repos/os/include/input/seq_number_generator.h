/*
 * \brief  Sequence number generator
 * \author Johannes Schlatow
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SEQ_NUMBER_GENERATOR_H_
#define _INCLUDE__SEQ_NUMBER_GENERATOR_H_

/* Genode includes */
#include <input/event.h>
#include <input/component.h>

namespace Input {

	struct Seq_number_generator;
}

struct Input::Seq_number_generator
{
	bool              _clicked    { false };
	bool              _submitted  { false };
	Input::Seq_number _seq_number { };

	static bool click(Input::Event const &event)
	{
		return event.key_press  (Input::BTN_LEFT) || event.key_press  (Input::BTN_TOUCH);
	}

	static bool clack(Input::Event const &event)
	{
		return event.key_release(Input::BTN_LEFT) || event.key_release(Input::BTN_TOUCH);
	}

	void new_seq_number()
	{
		_seq_number.value++;
		_submitted = false;
	}

	unsigned value() const {
		return _seq_number.value; }

	void apply_event(Input::Event const &event)
	{
		bool const orig_clicked = _clicked;

		if (click(event)) _clicked = true;
		if (clack(event)) _clicked = false;

		if (!orig_clicked && _clicked)
			new_seq_number();
	}

	void submit(Input::Session_component &input)
	{
		if (_submitted)
			return;

		input.submit(_seq_number);
		_submitted = true;
	}
};

#endif /* _INCLUDE__SEQ_NUMBER_GENERATOR_H_ */
