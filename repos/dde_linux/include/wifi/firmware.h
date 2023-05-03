/*
 * \brief  Firmware access interface
 * \author Josef Soentgen
 * \date   2023-05-05
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__FIRMWARE_H_
#define _WIFI__FIRMWARE_H_

#include <util/interface.h>

namespace Wifi {

	struct Firmware_request : Genode::Interface
	{
		enum State { INVALID, PROBING, PROBING_COMPLETE,
		             REQUESTING, REQUESTING_COMPLETE };
		State state { INVALID };
		bool success { false };

		/* name of the firmware image requested by the driver */
		char const *name { nullptr };

		/*
		 * Length of the firmware image in bytes used for
		 * arranging the memory buffer for the loaded firmware.
		 */
		unsigned long fw_len { 0 };

		/*
		 * Pointer to and length of the memory location where
		 * the firmware image should be copied into to. It is
		 * allocated by the driver.
		 */
		char          *dst      { nullptr };
		unsigned long  dst_len  { 0 };

		virtual void submit_response() = 0;
	};

	struct Firmware_request_handler : Genode::Interface
	{
		virtual void submit_request() = 0;
	};

	void firmware_establish_handler(Firmware_request_handler &);
	Firmware_request *firmware_get_request();

} /* namespace Wifi */

#endif /* _WIFI__FIRMWARE_H_ */
