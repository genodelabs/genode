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


namespace Wifi {

	enum { RFKILL_FD = 42, };
}

bool wifi_get_rfkill(void);
void wifi_set_rfkill(bool);

#endif /* _WIFI__RFKILL_H_ */
