/*
 * \brief  View that represents the origin of the pointer coordinate system
 * \author Norman Feske
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POINTER_ORIGIN_H_
#define _POINTER_ORIGIN_H_

#include "view.h"
#include "session.h"

struct Pointer_origin : Session, View
{
	Pointer_origin()
	:
		Session(Genode::Session_label()),
		View(*this, View::TRANSPARENT, View::NOT_BACKGROUND, 0)
	{ }


	/***********************
	 ** Session interface **
	 ***********************/

	void submit_input_event(Input::Event) override { }
	void submit_sync() override { }


	/********************
	 ** View interface **
	 ********************/

	int frame_size(Mode const &) const override { return 0; }
	void frame(Canvas_base &, Mode const &) const override { }
	void draw(Canvas_base &, Mode const &) const override { }
};

#endif /* _POINTER_ORIGIN_H_ */
