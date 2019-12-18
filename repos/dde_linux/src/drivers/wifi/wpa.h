/*
 * \brief  Wpa_supplicant thread of the wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__WPA_H_
#define _WIFI__WPA_H_


namespace Genode {
	struct Env;
	struct Lock;
}

class Wpa_thread
{
	private:

		Genode::Lock &_lock;
		int           _exit;

		static void * _entry_trampoline(void *arg);

		void _entry();

	public:

		Wpa_thread(Genode::Env &env, Genode::Lock &lock);
};

#endif /* _WIFI__WPA_H_ */
