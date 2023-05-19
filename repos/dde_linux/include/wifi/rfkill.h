/*
 * \brief  RFKILL interface
 * \author Josef Soentgen
 * \date   2018-07-11
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__RFKILL_H_
#define _WIFI__RFKILL_H_

#include <util/interface.h>

namespace Wifi {

	/*
	 * FD used to poll RFKILL state from the supplicant.
	 */
	enum { RFKILL_FD = 42, };

	struct Rfkill_notification_handler : Genode::Interface
	{
		virtual void rfkill_notify() = 0;
	};

	void rfkill_establish_handler(Rfkill_notification_handler &);

	/**
	 * Query current RFKILL state
	 */
	bool rfkill_blocked();

	/**
	 * Set RFKILL state from the frontend
	 *
	 * May be only called from an EP context.
	 */
	void set_rfkill(bool);

} /* namespace Wifi */

#endif /* _WIFI__RFKILL_H_ */
