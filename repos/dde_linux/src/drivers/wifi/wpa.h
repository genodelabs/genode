/*
 * \brief  Wpa_supplicant thread of the wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WIFI__WPA_H_
#define _WIFI__WPA_H_

/* Genode includes */
#include <base/sleep.h>

/* entry function */
extern "C" int wpa_main(void);
extern "C" void wpa_conf_reload(void);

class Wpa_thread : public Genode::Thread<8 * 1024 * sizeof(long)>
{
	private:

		Genode::Lock &_lock;
		int _exit;

	public:

		Wpa_thread(Genode::Lock &lock)
		: Thread("wpa_supplicant"), _lock(lock), _exit(-1) { }

		void entry()
		{
			/* wait until the wifi driver is up and running */
			_lock.lock();
			_exit = wpa_main();
			Genode::sleep_forever();
		}
};

#endif /* _WIFI__WPA_H_ */
