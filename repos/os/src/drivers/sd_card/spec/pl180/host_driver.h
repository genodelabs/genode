/*
 * \brief  SD-card host-interface driver
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HOST_DRIVER_H_
#define _HOST_DRIVER_H_

struct Host_driver
{
	/**
	 * Send request
	 *
	 * \param cmd       command index
	 * \param out_resp  response for command
	 *                  (set to 0 if not expected)
	 */
	virtual void request(unsigned char  cmd,
	                     unsigned      *out_resp) = 0;

	/**
	 * Send request
	 *
	 * \param cmd       command index
	 * \param arg       command argument
	 * \param out_resp  response for command
	 *                  (set to 0 if not expected)
	 */
	virtual void request(unsigned char  cmd,
	                     unsigned       arg,
	                     unsigned      *out_resp) = 0;

	/**
	 * Send data-read request
	 *
	 * \param cmd       command index
	 * \param arg       command argument
	 * \param length    number of bytes in data transfer
	 * \param out_resp  response for command
	 *                  (set to 0 if not expected)
	 */
	virtual void read_request(unsigned char  cmd,
	                          unsigned       arg,
	                          unsigned       length,
	                          unsigned      *out_resp) = 0;

	/**
	 * Send data-write request
	 *
	 * \param cmd       command index
	 * \param arg       command argument
	 * \param length    number of bytes in data transfer
	 * \param out_resp  response for command
	 *                  (set to 0 if not expected)
	 */
	virtual void write_request(unsigned char  cmd,
	                           unsigned       arg,
	                           unsigned       length,
	                           unsigned      *out_resp) = 0;

	/**
	 * Read data
	 *
	 * Used to read the data after a successful data-read request.
	 */
	virtual void read_data(unsigned  length,
	                       char     *out_buffer) = 0;

	/**
	 * Write data
	 *
	 * Used to write the data after a successful data-write request.
	 */
	virtual void write_data(unsigned    length,
	                        char const *buffer) = 0;
};

#endif /* _HOST_DRIVER_H_ */
