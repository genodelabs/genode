/*
 * \brief  Wpa_supplicant CTRL interface
 * \author Josef Soentgen
 * \date   2018-07-31
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__CTRL_H_
#define _WIFI__CTRL_H_

#include <util/string.h>

namespace Wifi {

	/*
	 * FD used to poll CTRL state from the supplicant.
	 */
	enum { CTRL_FD = 51, };

	struct Msg_buffer;

	struct Notify_interface : Genode::Interface
	{
		virtual void submit_response() = 0;
		virtual void submit_event() = 0;
		virtual void block_for_processing() = 0;
	};

	void ctrl_init(Msg_buffer &);

} /* namespace Wifi */


struct Wifi::Msg_buffer
{
	char send[4096];
	unsigned send_id;

	char recv[4096*8];
	unsigned recv_id;
	unsigned last_recv_id;

	char event[1024];
	unsigned event_id;
	unsigned last_event_id;

	Notify_interface &_notify;

	Msg_buffer(Notify_interface &notify)
	: _notify { notify } { }

	/*
	 * Member functions below are called by the
	 * CTRL interface.
	 */

	void notify_response() const {
		_notify.submit_response(); }

	void notify_event() const {
		_notify.submit_event(); }

	void block_for_processing() {
		_notify.block_for_processing(); }

	/*
	 * Member functions below are called by the
	 * Manager.
	 */

	void with_new_reply(auto const &fn)
	{
		char const *msg = reinterpret_cast<char const*>(recv);
		/* return early */
		if (last_recv_id == recv_id)
			return;

		last_recv_id = recv_id;
		fn(msg);
	}

	void with_new_event(auto const &fn)
	{
		char const *msg = reinterpret_cast<char const*>(event);

		if (last_event_id == event_id)
			return;

		last_event_id = event_id;
		fn(msg);
	}
};


void wpa_ctrl_set_fd(void);

#endif /* _WIFI__CTRL_H_ */
