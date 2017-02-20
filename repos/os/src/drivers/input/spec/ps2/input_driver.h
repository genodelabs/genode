/*
 * \brief  Input-driver interface
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__INPUT_DRIVER_H_
#define _DRIVERS__INPUT__SPEC__PS2__INPUT_DRIVER_H_

class Input_driver
{
	public:

		virtual void handle_event() = 0;

		virtual bool event_pending() const = 0;

		virtual ~Input_driver() { }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__INPUT_DRIVER_H_ */
