/*
 * \brief  TUN/TAP to Nic_session interface
 * \author Josef Soentgen
 * \date   2014-06-05
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#ifndef _TUNTAP_H_
#define _TUNTAP_H_

#include <base/stdint.h>


/**
 * This class handles the TUN/TAP access from OpenVPN's side
 */

struct Tuntap_device
{
	/**
	 * Read from TUN/TAP device
	 */
	virtual int read(char *buf, Genode::size_t len) = 0;

	/**
	 * Write to TUN/TAP device
	 */
	virtual int write(char const *buf, Genode::size_t len) = 0;

	/**
	 * Get file descriptor used to notify OpenVPN about incoming packets
	 */
	virtual int fd() = 0;

	/**
	 * Start-up lock up
	 */
	virtual void up() = 0;

	/**
	 * Start-up lock down
	 */
	virtual void down() = 0;
};

#endif /* _TUNTAP_H_ */
