/*
 * \brief  Wpa_supplicant thread of the wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WIFI__WPA_H_
#define _WIFI__WPA_H_

/* Genode includes */
#include <base/sleep.h>

/* entry function */
extern "C" int wpa_main(int);
extern "C" void wpa_conf_reload(void);

class Wpa_thread : public Genode::Thread
{
	private:

		Genode::Lock &_lock;
		int           _exit;
		bool          _debug_msg;

	public:

		Wpa_thread(Genode::Env &env, Genode::Lock &lock, bool debug_msg)
		:
			Thread(env, "wpa_supplicant", 8*1024*sizeof(long)),
			_lock(lock), _exit(-1), _debug_msg(debug_msg) { }

		void entry()
		{
			/* wait until the wifi driver is up and running */
			_lock.lock();
			_exit = wpa_main(_debug_msg);
			Genode::sleep_forever();
		}
};

#endif /* _WIFI__WPA_H_ */
